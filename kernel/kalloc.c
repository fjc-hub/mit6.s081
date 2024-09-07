// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
int pgidx(uint64 pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock; // protect freelist
  struct spinlock lockcounter; // protect counter
  struct run *freelist;
  char counter[PHYPGSZ]; // counter[i] represents the reference count of i'th physical page
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kmem.lockcounter, "kmemcounter");

  memset(kmem.counter, 0, sizeof(*kmem.counter)*PHYPGSZ);

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *pa;
  pa = (char*)PGROUNDUP((uint64)pa_start);
  for(; pa + PGSIZE <= (char*)pa_end; pa += PGSIZE) {
    struct run *r;

    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
      panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
}

// Free the page of physical memory pointed at by v,
// which should have been returned by kalloc
void
kfree(void *pa)
{
  int cnt, idx;
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lockcounter);
  idx = pgidx((uint64)pa);
  cnt = kmem.counter[idx];
  if (cnt < 0) {
    // release(&kmem.lockcounter); no need
    panic("kfree fast fail: invalid counter");
  } else if (cnt == 0) {
    // release(&kmem.lockcounter); no need
    panic("kfree fast fail: release page multiple times");
  } else if (cnt > 1) {
    kmem.counter[idx] = cnt - 1;
    release(&kmem.lockcounter);
    
    return;

  } else { // cnt == 1
    kmem.counter[idx] = 0;
    release(&kmem.lockcounter);

    // Fill with junk to catch dangling refs.
    // must POSITION before freelist-insertion Otherwise memset will overlap next pointer
    memset(pa, 1, PGSIZE);

    // insert the page into freelist
    acquire(&kmem.lock);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    // set ref counter to 1
    acquire(&kmem.lockcounter);
    kmem.counter[pgidx((uint64)r)] = 1;
    release(&kmem.lockcounter);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// return the index in counter array
int pgidx(uint64 pa) {
  if (pa < KERNBASE) {
    panic("panic:pgidx");
  }
  return (PGROUNDDOWN(pa) - KERNBASE) / PGSIZE;
}

// get the reference counter of physical page pointed to by pa
int pgrefc(uint64 pa) {
  int r;

  acquire(&kmem.lockcounter);
  r = (int) (kmem.counter[pgidx(pa)]);
  release(&kmem.lockcounter);
  return r;
}

// increase the reference counter of physical page pointed to by pa
int incrpgrefc(uint64 pa) {
  int cnt, idx;

  acquire(&kmem.lockcounter);
  idx = pgidx(pa);
  cnt = kmem.counter[idx];
  kmem.counter[idx] = cnt + 1;
  release(&kmem.lockcounter);
  
  return cnt;
}
// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(int idx, void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock locks[NCPU];
  struct run *freelists[NCPU];
} kmem;

void
kinit()
{
  int i;
  char *pa;
  uint64 start = PGROUNDUP((uint64)end), num;
  if ((PHYSTOP - start) % PGSIZE != 0)
    panic("kinit");
  
  pa = (char*)start;
  int n = (PHYSTOP - start) / PGSIZE / NCPU; // the number of page allocates to each cpu
  for(i=0; i < NCPU; i++) {
    initlock(&kmem.locks[i], "kmem");
    num = (uint64) n;
    if (i == NCPU - 1) {
      num += (PHYSTOP - start) / PGSIZE % NCPU;
    }
    // printf("%p %p\n" (void*)start, (void *)((char *)start + num * PGSIZE))
    freerange(i, (void *) pa, (void *)(pa + num * PGSIZE));
    pa += num * PGSIZE;
  }
}

void 
freerange(int idx, void *pa_start, void *pa_end)
{
  char *p;
  struct run *r;

  p = (char*)PGROUNDDOWN((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    if(((uint64)p % PGSIZE) != 0 || (char*)p < end || (uint64)p >= PHYSTOP)
      panic("freerange");

    // // Fill with junk to catch dangling refs.
    // memset(p, 1, PGSIZE);

    r = (struct run*)p;

    // acquire(&kmem.locks[idx]);
    r->next = kmem.freelists[idx];
    kmem.freelists[idx] = r;
    // release(&kmem.locks[idx]);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int hart = cpuid();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.locks[hart]);
  r->next = kmem.freelists[hart];
  kmem.freelists[hart] = r;
  release(&kmem.locks[hart]);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  int i;
  struct run *r;
  int hart = cpuid();

  acquire(&kmem.locks[hart]);
  r = kmem.freelists[hart];
  if(r) {
    kmem.freelists[hart] = r->next;
  } else {
    // steal free page from other cpu's freelist
    for(i=0; i < NCPU; i++) {
      if (i == hart) {
        continue;
      }
      acquire(&kmem.locks[i]);
      r = kmem.freelists[i];
      if(r) {
        kmem.freelists[i] = r->next;
        release(&kmem.locks[i]);
        break;
      }
      release(&kmem.locks[i]);
    }
  }
  release(&kmem.locks[hart]);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

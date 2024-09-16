// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

/* hash table interface */
struct buf* htget(hashtable ht, uint64 key);
struct buf* htremove(struct hashbucket* bucket, struct buf* evict);
void htadd(hashtable ht, struct buf *et, uint64 key);
struct buf* htget_sync(hashtable ht, uint64 key); // get the buf pointer by key and lock buf if existed

hashtable myht; // hashtable

struct {
  // struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  int i;
  struct buf *b;

  // init hashtable myht
  for(i=0; i < HASHSIZE; i++) {
    initlock(&myht[i].lock, "bcache");
    myht[i].head = 0;
  }

  // init buffers
  i=0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->ticks = 0;
    b->htnext = 0;
    htadd(myht, b, (uint64)(i++));
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int i, fri=-1;
  uint mticks = 0xffffff80;
  struct buf *srcbukt, *freebukt=0, *p;
  uint64 key = HASHKEY(dev, blockno);
  int idx = HASHFUNC(key);

  // Query cache by hash
  srcbukt = htget_sync(myht, key); 
  /** cache hit **/
  if (srcbukt) {
    return srcbukt;
  }

  /** cache miss **/
  /* Optimize Tips: you can search unused block buffer in lock-acquired buckets first, if not, search other buckets */
  // get the least recently used (LRU) unused buffer and lock its bucket. locking and searching bucket one by one
  int skip;
  for(i=0; i < HASHSIZE; i++) {
    skip=0;
    acquire(&myht[i].lock); // lock bucket
    for (p = myht[i].head; p; p=p->htnext) {
      if (p->refcnt == 0 && p->ticks < mticks) {
        if (fri >= 0 && fri != i) {// avoid release myht[idx].lock
          release(&myht[fri].lock); // release last LRU block-buf's bucket
        }
        mticks = p->ticks;
        freebukt = p;
        fri = i;
        skip = 1; // skip release, lock temporarily current LRU block-buf's bucket
      }
    }
    if (!skip) release(&myht[i].lock); // release bucket
  }
  if (!freebukt) panic("bget: no block buffers");

  if (htremove(&myht[fri], freebukt) == 0) // remove old version of freebukt from cache
    panic("htremove failed");
  release(&myht[fri].lock); // release reused buffer's bucket

  /* Optimized Tips: if fri == idx, don't need to release(&myht[fri].lock) and acquire myht[idx].lock in htget_sync(myht, key, 1)*/

  /* after htremove, the Evicted Buffer-Block is no longer shared by threads, it's private to current thread*/

  // re-acquire myht[idx].lock and check if this block-buffer added by other threads
  acquire(&myht[idx].lock);
  srcbukt = htget(myht, key); 
  if (srcbukt) {
    /* key-block had already been added into key-bucket by certain other thread */

    // Inserting Evicted-And-Removed block-buffer back into Hash-Buffer-Cache 
    // For avoiding making Cache smaller and smaller 
    freebukt->refcnt = 0;
    freebukt->key = REVERSEKEY;
    freebukt->htnext = myht[idx].head;
    myht[idx].head = freebukt;
    
    srcbukt->refcnt++; // increase count

    release(&myht[idx].lock);
    acquiresleep(&srcbukt->lock); // lock added block srcbukt
    return srcbukt;
  } else {
    /* key-block has not been added into key-bucket */
    // reuse block freebukt
    freebukt->dev = dev;
    freebukt->blockno = blockno;
    freebukt->valid = 0;
    freebukt->refcnt = 1;
    htadd(myht, freebukt, key); // add freebukt into cache 

    release(&myht[idx].lock);
    acquiresleep(&freebukt->lock); // lock evicted block freebukt
    return freebukt;
  }

  panic("绝对不可能");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer. cannot be used if refcnt decreases to 0
// set the unused block with current cpu ticks
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int idx = HASHFUNC(b->key);

  acquire(&myht[idx].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // update ticks (obey the origin version logic: make block-buffer used only when its refcnt equals to 0)
    acquire(&tickslock);
    b->ticks = ticks;
    release(&tickslock);

    /* delay remove operation until bget query for unused block-buffer
       may slow the query speed of hashtable, because there are many refcnt==0 block-buffers in table */
    // remove from hashtable
  }
  
  release(&myht[idx].lock);
}

void
bpin(struct buf *b) {
  int idx = HASHFUNC(b->key);
  acquire(&myht[idx].lock);
  b->refcnt++;
  release(&myht[idx].lock);
}

void
bunpin(struct buf *b) {
  int idx = HASHFUNC(b->key);
  acquire(&myht[idx].lock);
  b->refcnt--;
  release(&myht[idx].lock);
}

// query entry from ht by key
struct buf * htget(hashtable ht, uint64 key) {
  int idx = HASHFUNC(key);
  struct buf *p;
  p = ht[idx].head;
  while (p != 0) {
    if (p->key == key) {
      return p;
    }
    p = p->htnext;
  }
  return 0;
}

// remove entry from hash bucket by entry's address, return the removed entry's address
struct buf* htremove(struct hashbucket* bucket, struct buf* evict) {
  if (bucket == 0)
    return 0;

  if (bucket->head == evict) {
    bucket->head = evict->htnext;
    evict->htnext = 0;
    return evict;
  }

  struct buf *pp, *p;
  pp = bucket->head;
  p = pp->htnext;

  while (p != 0 && p != evict) {
    pp = p;
    p = p->htnext;
  }
  if (!p) {
    return 0;
  }
  pp->htnext = p->htnext;
  p->htnext = 0;
  return p;
}

// add an entry into ht
void htadd(hashtable ht, struct buf *et, uint64 key) {
  int idx = HASHFUNC(key);

  et->key = key;
  et->htnext = ht[idx].head;
  ht[idx].head = et;
}

// query concurrent-safely entry from ht by key, and lock returned buf. lockbucket=1 donnot release bucket-lock
struct buf * htget_sync(hashtable ht, uint64 key) {
  int idx;
  struct buf *p;
  idx = HASHFUNC(key);
  
  acquire(&ht[idx].lock);
  p = htget(ht, key); 

  /** cache hit **/
  if (p) {
    p->refcnt++;

    release(&ht[idx].lock);
    acquiresleep(&p->lock);
    return p;
  }

  release(&ht[idx].lock);
  return 0;
}
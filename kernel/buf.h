struct buf {
  /* protected concurrently by sleeplock lock */
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uchar data[BSIZE];

  /* protected concurrently by the spinlock in bcache
     remove this linked suit of LRU, because is's less lock-contention by using time-stamps LRU */
  // uint refcnt;
  // struct buf *prev; // LRU cache list
  // struct buf *next;

  /* protected concurrently by hashtable's Bucket-Lock */
  uint64 key; // key for hashtable, key=HASHKEY(dev, blockno)
  struct buf *htnext; // pointer for hashtable
  uint refcnt;
  uint ticks;
};

struct hashbucket{
  struct spinlock lock;
  struct buf *head;
};

#define HASHSIZE 17 // the number of buckets of hash table

typedef struct hashbucket hashtable[HASHSIZE];

// hash function, key=HASHKEY(dev, blockno)
#define HASHFUNC(x) ((x) % HASHSIZE)

#define HASHKEY(dev, blockno) ((((uint64)(dev)) << 32) | (blockno))
struct buf {
  uint ticks_buf;
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
//  struct spinlock *bucket_lock;
  uchar data[BSIZE];
};


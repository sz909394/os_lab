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

#define NBUCKET 13
#define table_size 1000
extern uint ticks;

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf *table[table_size][NBUCKET];
//  struct spinlock lock_bucket[NBUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
//  struct buf head;
} bcache;

void
binit(void)
{
//  struct buf *b;

  initlock(&bcache.lock, "bcache");
#if 0
  for(int i = 0; i < NBUCKET; i++)
  {
//    initlock(&bcache.lock_bucket[i], "bcache.bucket");
//    memset(&bcache.table[i], 0, NBUF);
  }
#endif
  for(int i = 0; i < NBUF; i++)
  {
    initsleeplock(&bcache.buf[i].lock, "buffer");
  }
#if 0
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
#endif
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b = 0;
  int bucket_index = blockno % NBUCKET;
  int table_index = blockno % table_size;

//  acquire(&bcache.lock_bucket[bucket_index]);
  acquire(&bcache.lock);
  b = bcache.table[table_index][bucket_index];
  if(b)
  {
    if((b->dev != dev) || (b->blockno != blockno)){
      printf("b->dev is %d, b->blockno is %d\n", b->dev, b->blockno);
      printf("dev is %d, blockno is %d\n", dev, blockno);
      printf("bucket_index is %d, table_index is %d\n", bucket_index, table_index);
      panic("bget dev blockno hashtable conflicts");
    }
    b->refcnt++;
//    b->bucket_lock = &bcache.lock_bucket[bucket_index];
//    release(&bcache.lock_bucket[bucket_index]);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }
  else
  {
#if 0
    acquire(&bcache.lock);
    for(int i = 0; i < NBUCKET; i++)
    {
      if(bucket_index != i)
        acquire(&bcache.lock_bucket[i]);
    }
#endif
    for(int i = 0; i < NBUF; i++)
    {
      if(!b && bcache.buf[i].refcnt == 0)
        b = &bcache.buf[i];
      else if((bcache.buf[i].refcnt == 0) && (bcache.buf[i].ticks_buf >= b->ticks_buf))
        b = &bcache.buf[i];
    }

    if(b)
    {
      int rm_bucket_index = b->blockno % NBUCKET;
      int rm_table_index = b->blockno % table_size;
      bcache.table[rm_table_index][rm_bucket_index] = 0;

      bcache.table[table_index][bucket_index] = b;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
//      b->bucket_lock = &bcache.lock_bucket[bucket_index];
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
#if 0
    for(int i = 0; i < NBUCKET; i++)
    {
      if(bucket_index != i)
        release(&bcache.lock_bucket[i]);
    }
    release(&bcache.lock);
    release(&bcache.lock_bucket[bucket_index]);
#endif
  }
  panic("bget: no buffers");
}
#if 0
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}
#endif
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

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

//  acquire(b->bucket_lock);
  acquire(&bcache.lock);
  if(b->refcnt == 0)
    panic("brelse, refcnt is already 0");
  b->refcnt--;
  b->ticks_buf = ticks;
  release(&bcache.lock);
//  release(b->bucket_lock);
}
#if 0
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}
#endif

void
bpin(struct buf *b) {
//  acquire(b->bucket_lock);
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
//  release(b->bucket_lock);
}

void
bunpin(struct buf *b) {
//  acquire(b->bucket_lock);
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
//  release(b->bucket_lock);
}

#if 0
void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}
#endif


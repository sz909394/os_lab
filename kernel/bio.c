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

struct
{
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf *table[table_size][NBUCKET];
  struct spinlock lock_bucket[NBUCKET];
} bcache;

void binit(void)
{
  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; i++)
  {
    initlock(&bcache.lock_bucket[i], "bcache.bucket");
  }
  for (int i = 0; i < NBUF; i++)
  {
    initsleeplock(&bcache.buf[i].lock, "buffer");
    bcache.buf[i].buck_id = -1;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b = 0;
  int bucket_index = blockno % NBUCKET;
  int table_index = blockno % table_size;

  acquire(&bcache.lock_bucket[bucket_index]);
  b = bcache.table[table_index][bucket_index];
  if (b)
  {
    if ((b->dev != dev) || (b->blockno != blockno))
    {
      printf("b->dev is %d, b->blockno is %d\n", b->dev, b->blockno);
      printf("dev is %d, blockno is %d\n", dev, blockno);
      printf("bucket_index is %d, table_index is %d\n", bucket_index, table_index);
      panic("bget dev blockno hashtable conflicts");
    }
    b->refcnt++;
    b->bucket_lock = &bcache.lock_bucket[bucket_index];
    b->buck_id = bucket_index;
    release(&bcache.lock_bucket[bucket_index]);
    acquiresleep(&b->lock);
    return b;
  }
  else
  {
    release(&bcache.lock_bucket[bucket_index]);
    acquire(&bcache.lock);
    {
      // 在上面放开锁的一瞬间，也许有另外一个进程已经进来帮忙分配了这个 buf, 需要 double check.
      acquire(&bcache.lock_bucket[bucket_index]);
      b = bcache.table[table_index][bucket_index];
      if (b)
      {
        b->refcnt++;
        b->bucket_lock = &bcache.lock_bucket[bucket_index];
        b->buck_id = bucket_index;
        release(&bcache.lock_bucket[bucket_index]);
        release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
      }
      release(&bcache.lock_bucket[bucket_index]);
    }
    for (int i = 0; i < NBUCKET; i++)
    {
      acquire(&bcache.lock_bucket[i]);
      for (int j = 0; j < NBUF; j++)
      {
        if (bcache.buf[j].buck_id == -1)
        {
          b = &bcache.buf[j];
          break;
        }
        if (!b && (bcache.buf[j].buck_id == i) && (bcache.buf[j].refcnt == 0))
          b = &bcache.buf[j];
        else if (b && (bcache.buf[j].buck_id == i) && (bcache.buf[j].refcnt == 0) && (bcache.buf[j].ticks_buf >= b->ticks_buf))
          b = &bcache.buf[j];
      }
      if (b)
      {
        if (b->buck_id != -1)
        {
          int rm_bucket_index = b->blockno % NBUCKET;
          int rm_table_index = b->blockno % table_size;
          if ((rm_bucket_index == bucket_index) && (rm_table_index == table_index))
            panic(" rm_bucket_index and bucket_index is the same");
          if (rm_bucket_index != b->buck_id)
            panic("rm_bucket_index is not the same as b->buck_id");
          bcache.table[rm_table_index][rm_bucket_index] = 0;
        }
        if (i != bucket_index)
        {
          release(&bcache.lock_bucket[i]);
          acquire(&bcache.lock_bucket[bucket_index]);
        }
        bcache.table[table_index][bucket_index] = b;
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->buck_id = bucket_index;
        b->bucket_lock = &bcache.lock_bucket[bucket_index];
        release(&bcache.lock_bucket[bucket_index]);
        release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
      }
      release(&bcache.lock_bucket[i]);
    }
    release(&bcache.lock);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(b->bucket_lock);
  if (b->refcnt == 0)
    panic("brelse, refcnt is already 0");
  b->refcnt--;
  if (b->refcnt == 0)
    b->ticks_buf = ticks;
  release(b->bucket_lock);
}

void bpin(struct buf *b)
{
  acquire(b->bucket_lock);
  b->refcnt++;
  release(b->bucket_lock);
}

void bunpin(struct buf *b)
{
  acquire(b->bucket_lock);
  b->refcnt--;
  release(b->bucket_lock);
}

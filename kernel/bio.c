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

#define NBUCKET 13      // the count of hash table's buckets
#define HASH(blockno) (blockno % NBUCKET)


struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf buckets[NBUCKET]; 
  struct spinlock bucketlocks[NBUCKET];   // buckets' locks 
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
} bcache;

int getH(uint blockno) {
    return HASH(blockno);  // 使用宏定义 HASH(blockno % NBUCKET) 来计算哈希值
}

int getHb(struct buf *b) {
    return HASH(b->blockno);  // 从缓冲区结构体中获取 blockno 并计算哈希值
}


void
binit(void)
{
  struct buf *b;
  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; i++)
  {
    initlock(&bcache.bucketlocks[i], "bcache.bucket");
    bcache.buckets[i].prev = &bcache.buckets[i];
    bcache.buckets[i].next = &bcache.buckets[i];
  }

  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    int hash = getHb(b);
    b->time_stamp = ticks;
    b->next = bcache.buckets[hash].next;
    b->prev = &bcache.buckets[hash];
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[hash].next->prev = b;
    bcache.buckets[hash].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int hash = getH(blockno);
  struct buf *b;
 
  acquire(&bcache.bucketlocks[hash]);

  for(b = bcache.buckets[hash].next; b != &bcache.buckets[hash]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->time_stamp = ticks;
      b->refcnt++;
      //printf("## end has \n");
      release(&bcache.bucketlocks[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // If there is no cached buffer for the given sector, bget must make one, possibly reusing a buffer
  // that held a different sector. It scans the buffer list a second time, looking for a buffer that 
  // is not in use
  // (b->refcnt = 0); any such buffer can be used. Bget edits the buffer metadata to record the new
  // device and sector number and acquires its sleep-lock. Note that the assignment b->valid = 0
  // ensures that bread will read the block data from disk rather than incorrectly using the buffer’s
  // previous contents.
  // Not cached; recycle an unused buffer.

  for (int i = 0; i < NBUCKET; i++)
  {
    if(i != hash){
      acquire(&bcache.bucketlocks[i]);
      for(b = bcache.buckets[i].prev; b != &bcache.buckets[i]; b = b->prev){
        if(b->refcnt == 0){
          b->time_stamp = ticks;
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;     //important  
          b->refcnt = 1;
          
          /** 将b脱出  */
          b->next->prev = b->prev;
          b->prev->next = b->next;
          
          /** 将b接入  */
          b->next = bcache.buckets[hash].next;
          b->prev = &bcache.buckets[hash];
          bcache.buckets[hash].next->prev = b;
          bcache.buckets[hash].next = b;
          //printf("## end alloc: hash: %d, has: %d\n", hash,i);
          release(&bcache.bucketlocks[i]);
          release(&bcache.bucketlocks[hash]);
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&bcache.bucketlocks[i]);
    }
  }
  panic("bget: no buffers");
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

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int blockno = getHb(b);
  b->time_stamp = ticks;
  if(b->time_stamp == ticks){
    b->refcnt--;
    if(b->refcnt == 0){
      /** 将b脱出  */
      b->next->prev = b->prev;
      b->prev->next = b->next;
      
      /** 将b接入  */
      b->next = bcache.buckets[blockno].next;
      b->prev = &bcache.buckets[blockno];
      bcache.buckets[blockno].next->prev = b;
      bcache.buckets[blockno].next = b;
    }
  }
}

void
bpin(struct buf *b) {
  int index = HASH(b->blockno);
  acquire(&bcache.bucketlocks[index]);
  b->refcnt++;
  release(&bcache.bucketlocks[index]);
}

void
bunpin(struct buf *b) {
  int index = HASH(b->blockno);
  acquire(&bcache.bucketlocks[index]);
  b->refcnt--;
  release(&bcache.bucketlocks[index]);
}



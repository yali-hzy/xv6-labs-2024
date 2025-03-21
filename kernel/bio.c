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
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf bucket[NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  char name[32];

  initlock(&bcache.lock, "bcache");

  for(int i = 0; i < NBUCKET; i++) {
    snprintf(name, sizeof(name), "bcache_bucket_lock_%d", i);
    initlock(&bcache.bucket_lock[i], name);
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.bucket[i].next = &bcache.bucket[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bucket[0].next;
    b->prev = &bcache.bucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[0].next->prev = b;
    bcache.bucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // Is the block already cached?
  int hashcode = blockno % NBUCKET;
  acquire(&bcache.bucket_lock[hashcode]);
  for(b = bcache.bucket[hashcode].next; b != &bcache.bucket[hashcode]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[hashcode]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[hashcode]);

  // Not cached.
  acquire(&bcache.lock);
  acquire(&bcache.bucket_lock[hashcode]);
  for(b = bcache.bucket[hashcode].next; b != &bcache.bucket[hashcode]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[hashcode]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  for(int hashcode2 = 0; hashcode2 < NBUCKET; hashcode2++) {
    if(hashcode2 != hashcode)
      acquire(&bcache.bucket_lock[hashcode2]);
    for(b = bcache.bucket[hashcode2].next; b != &bcache.bucket[hashcode2]; b = b->next){
      if(b->refcnt == 0){
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.bucket[hashcode].next;
        b->prev = &bcache.bucket[hashcode];
        bcache.bucket[hashcode].next->prev = b;
        bcache.bucket[hashcode].next = b;
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        if(hashcode2 != hashcode)
          release(&bcache.bucket_lock[hashcode2]);
        release(&bcache.bucket_lock[hashcode]);
        release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    if(hashcode2 != hashcode)
      release(&bcache.bucket_lock[hashcode2]);
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

  int hashcode = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[hashcode]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[hashcode].next;
    b->prev = &bcache.bucket[hashcode];
    bcache.bucket[hashcode].next->prev = b;
    bcache.bucket[hashcode].next = b;
  }
  
  release(&bcache.bucket_lock[hashcode]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.bucket_lock[b->blockno % NBUCKET]);
  b->refcnt++;
  release(&bcache.bucket_lock[b->blockno % NBUCKET]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.bucket_lock[b->blockno % NBUCKET]);
  b->refcnt--;
  release(&bcache.bucket_lock[b->blockno % NBUCKET]);
}



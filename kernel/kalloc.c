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

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int ref[PHYSTOP/PGSIZE];
} page_ref;

// op: 0 -> dec then read, 1 -> inc, other -> unknown op
int
page_ref_op(int op, uint64 pa)
{
  int value = 0;
  acquire(&page_ref.lock);
  switch(op){
    case 0:
      page_ref.ref[pa/PGSIZE] -= 1;
      value = page_ref.ref[pa/PGSIZE];
      break;
    case 1:
      page_ref.ref[pa/PGSIZE] += 1;
      break;
    default:
      panic("page_ref_op, unknown op");
  }
  release(&page_ref.lock);
  return value;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&page_ref.lock, "page_ref");
  memset(page_ref.ref, 0, sizeof(page_ref.ref));
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    page_ref_op(1, (uint64)p);
    kfree(p);
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(page_ref_op(0, (uint64)pa) == 0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    page_ref_op(1, (uint64)r);
  }
  return (void*)r;
}

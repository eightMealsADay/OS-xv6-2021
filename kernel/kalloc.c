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
  char lockname[8];  // save lock's name
} kmems[NCPU];

void
kinit()
{
  int i=0;
  for (i = 0; i < NCPU; ++i) {
    snprintf(kmems[i].lockname, 8, "kmem_%d", i);    // the name of the lock
    initlock(&kmems[i].lock, kmems[i].lockname);
  }
//  initlock(&kmem.lock, "kmem"); 
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int _cpuid;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // get the current core number - lab8-1
  push_off();
  _cpuid = cpuid();
  pop_off();

  // free the page to the current cpu's freelist
  acquire(&kmems[_cpuid].lock);
  r->next = kmems[_cpuid].freelist;
  kmems[_cpuid].freelist = r;
  release(&kmems[_cpuid].lock);
}

// steal half page from other cpu's freelist
struct run *steal(int cpu_id) {
    int i;
    int _cpuid = cpu_id;
    struct run *fast, *slow, *head;
    // 若传递的cpuid和实际运行的cpuid出现不一致,则引发panic
    // 加入该判断以检查在kalloc()调用steal时CPU不会被切换
    if(cpu_id != cpuid()) {
      panic("steal");
    }    
    // 遍历其他NCPU-1个CPU的空闲物理页链表 
    for (i = 1; i < NCPU; ++i) {
        if (++_cpuid == NCPU) {
            _cpuid = 0;
        }
        acquire(&kmems[_cpuid].lock);
        // 若链表不为空
        if (kmems[_cpuid].freelist) {
            // 快慢双指针算法将链表一分为二
            slow = head = kmems[_cpuid].freelist;
            fast = slow->next;
            while (fast) {
                fast = fast->next;
                if (fast) {
                    slow = slow->next;
                    fast = fast->next;
                }
            }
            // 后半部分作为当前CPU的空闲链表
            kmems[_cpuid].freelist = slow->next;
            release(&kmems[_cpuid].lock);
            // 前半部分的链表结尾清空,由于该部分链表与其他链表不再关联,因此无需加锁
            slow->next = 0;
            // 返回前半部分的链表头
            return head;
        }
        release(&kmems[_cpuid].lock);
    }
    // 若其他CPU物理页均为空则返回空指针
    return 0;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int _cpuid;

  push_off();
  _cpuid = cpuid();
  pop_off();

  // get the page from the current cpu's freelist
  acquire(&kmems[_cpuid].lock);
  r = kmems[_cpuid].freelist;
  if(r)
    kmems[_cpuid].freelist = r->next;
  release(&kmems[_cpuid].lock);
  // steal page - lab8-1
  // 若当前CPU空闲物理页为空,且偷取到了物理页
  if(!r && (r = steal(_cpuid))) {
    // 加锁修改当前CPU空闲物理页链表
    acquire(&kmems[_cpuid].lock);
    kmems[_cpuid].freelist = r->next;
    release(&kmems[_cpuid].lock);
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}

// sem.c: counting semaphores inside the xv6 kernel (Lab 13).
// A small table of NSEM semaphores, shared by every process.
// Each one is a number plus a spinlock that protects the number.

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"

#define NSEM 8

struct sem {
  struct spinlock lock; // a zeroed spinlock is a free spinlock
  int value;            // how many may still pass
};

static struct sem sems[NSEM];

// Look up semaphore id from syscall arg 0; returns 0 if out of range.
static struct sem *
fetch_sem(void)
{
  int id;
  argint(0, &id);
  if (id < 0 || id >= NSEM)
    return 0;
  return &sems[id];
}

// sem_init(id, value): give semaphore id its start value.
uint64
sys_sem_init(void)
{
  struct sem *s = fetch_sem();
  int value;
  argint(1, &value);
  if (!s || value < 0)
    return -1;
  acquire(&s->lock);
  s->value = value;
  release(&s->lock);
  return 0;
}

// sem_down(id): take one unit; if there is none, sleep until there is.
uint64
sys_sem_down(void)
{
  struct sem *s = fetch_sem();
  if (!s)
    return -1;

  acquire(&s->lock);
  while (s->value == 0) {
    sleep_prepare(s);       // I will sleep on the address of s
    release(&s->lock);      // never sleep holding a spinlock
    sleep();                // returns at once if sem_up came in between
    acquire(&s->lock);
    if (killed(myproc())) { // kill must still work on a waiting process
      release(&s->lock);
      return -1;
    }
  }
  s->value--;
  release(&s->lock);
  return 0;
}

// sem_up(id): give one unit back and wake whoever sleeps on s.
uint64
sys_sem_up(void)
{
  struct sem *s = fetch_sem();
  if (!s)
    return -1;

  acquire(&s->lock);
  s->value++;
  wakeup(s);
  release(&s->lock);
  return 0;
}

// sem_trydown(id): take one unit if > 0, or return -1 immediately without sleeping.
uint64
sys_sem_trydown(void)
{
  struct sem *s = fetch_sem();
  if (!s)
    return -1;

  acquire(&s->lock);
  if (s->value == 0) {
    release(&s->lock);
    return -1;
  }
  s->value--;
  release(&s->lock);
  return 0;
}


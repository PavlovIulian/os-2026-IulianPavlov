# Operating Systems -- Lab 18: Build a User-Level Thread Switch

**Course:** Operating Systems (Track B: build the OS (xv6))  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026--2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Architecture

### 1.1 Laboratory Objectives
In operating systems, threading can be implemented at two primary levels:
1. **Kernel-level threads (1:1 model)**: Managed directly by the OS kernel via system calls (`clone()`, `fork()`, kernel thread scheduling). Context switches require entering supervisor mode via traps or timer interrupts, saving full CPU state (`trapframe`), and switching kernel stacks.
2. **User-level threads (N:1 or green threads model)**: Managed entirely within user space by a runtime library without kernel involvement. Thread scheduling and context switching occur without executing supervisor traps or syscalls, yielding microsecond or sub-microsecond switch latencies.

The objective of Laboratory Work No. 18 is to construct a minimal user-level cooperative thread library (`uthread`) on xv6 RISC-V:
- Designing a saved CPU register context matching the RISC-V calling convention.
- Implementing an assembly routine (`thread_switch`) that saves and restores callee-saved registers.
- Initializing thread state and execution stacks in `thread_create`.
- Implementing a round-robin cooperative scheduler (`thread_schedule`) driven by explicit yields (`thread_yield`).

### 1.2 Context Switching Mechanics
Switching between threads requires preserving the state of execution of the outgoing thread and restoring that of the incoming thread. Because a user-level thread switch occurs synchronously at known function-call boundaries (inside `thread_schedule`), the compiler guarantees that all caller-saved registers (`t0`--`t6`, `a0`--`a7`) are already safely spilled to the current thread's stack.

Consequently, `thread_switch` only needs to preserve the **callee-saved registers**:
- `ra` (Return address register, $x1$)
- `sp` (Stack pointer, $x2$)
- `s0` / `fp` (Frame pointer / saved register 0, $x8$)
- `s1`--`s11` (Saved registers 1 through 11, $x9, x18$--$x27$)

This yields a context structure of exactly 14 64-bit words (112 bytes), completely identical in principle to the xv6 kernel's internal `struct context` in `kernel/swtch.S`.

---

## 2. Implementation Details

### 2.1 Register Context and Thread Descriptor (`user/uthread.c`)
Each thread maintains its execution context, a dedicated 8 KiB stack aligned to a 16-byte boundary, and a life-cycle state flag (`FREE`, `RUNNING`, `RUNNABLE`):

```c
#include "kernel/types.h"
#include "user/user.h"

#define FREE 0x0
#define RUNNING 0x1
#define RUNNABLE 0x2

#define STACK_SIZE 8192
#define MAX_THREAD 4

/* Callee-saved registers (14 fields, each uint64) */
struct context {
  uint64 ra;
  uint64 sp;

  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  struct context ctx;
  char stack[STACK_SIZE] __attribute__((aligned(16)));
  int state;
};

struct thread all_thread[MAX_THREAD];
struct thread *current_thread;
```

### 2.2 Assembly Context Switcher (`user/uthread_switch.S`)
The assembly routine `thread_switch(struct context *old, struct context *new)` takes pointers in registers `a0` and `a1`. It dumps the running thread's registers into `0(a0)`..`104(a0)`, loads the new thread's registers from `0(a1)`..`104(a1)`, and executes `ret`:

```assembly
	.text
	.globl thread_switch
thread_switch:
	sd ra, 0(a0)
	sd sp, 8(a0)
	sd s0, 16(a0)
	sd s1, 24(a0)
	sd s2, 32(a0)
	sd s3, 40(a0)
	sd s4, 48(a0)
	sd s5, 56(a0)
	sd s6, 64(a0)
	sd s7, 72(a0)
	sd s8, 80(a0)
	sd s9, 88(a0)
	sd s10, 96(a0)
	sd s11, 104(a0)

	ld ra, 0(a1)
	ld sp, 8(a1)
	ld s0, 16(a1)
	ld s1, 24(a1)
	ld s2, 32(a1)
	ld s3, 40(a1)
	ld s4, 48(a1)
	ld s5, 56(a1)
	ld s6, 64(a1)
	ld s7, 72(a1)
	ld s8, 80(a1)
	ld s9, 88(a1)
	ld s10, 96(a1)
	ld s11, 104(a1)

	ret
```

### 2.3 Thread Creation (`thread_create`)
When creating a thread, the thread's saved return address `ra` is initialized to the entry-point address of `func`, and its stack pointer `sp` is set to the high end of the stack (`t->stack + STACK_SIZE`):

```c
void
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  t->ctx.ra = (uint64)func;
  t->ctx.sp = (uint64)t->stack + STACK_SIZE;
}
```

### 2.4 Scheduler and Yielding (`thread_schedule`, `thread_yield`)
Scheduling employs a round-robin scan over `all_thread`:

```c
void
thread_schedule(void)
{
  struct thread *t, *next_thread;

  next_thread = 0;
  t = current_thread + 1;
  for(int i = 0; i < MAX_THREAD; i++){
    if(t >= all_thread + MAX_THREAD)
      t = all_thread;
    if(t->state == RUNNABLE) {
      next_thread = t;
      break;
    }
    t = t + 1;
  }

  if (next_thread == 0) {
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {
    struct thread *old = current_thread;
    next_thread->state = RUNNING;
    current_thread = next_thread;
    thread_switch(&old->ctx, &next_thread->ctx);
  }
}

void
thread_yield(void)
{
  current_thread->state = RUNNABLE;
  thread_schedule();
}
```

### 2.5 Cooperative Workloads (`thread_a`, `thread_b`, `thread_c`)
Each thread signals that it has started, awaits the startup of the other two threads by yielding, and then counts $0..99$, taking turns via `thread_yield()`:

```c
volatile int a_started, b_started, c_started;
volatile int a_n, b_n, c_n;

void
thread_a(void)
{
  int i;
  printf("thread_a started\n");
  a_started = 1;
  while(b_started == 0 || c_started == 0)
    thread_yield();

  for (i = 0; i < 100; i++) {
    printf("thread_a %d\n", i);
    a_n += 1;
    thread_yield();
  }
  printf("thread_a: exit after %d\n", a_n);
  current_thread->state = FREE;
  thread_schedule();
}
// thread_b and thread_c symmetrically implemented
```

### 2.6 Makefile Linker Integration
Because `_uthread` requires two separate translation units (`uthread.o` and `uthread_switch.o`), an explicit target rule is added to `Makefile`:

```make
UPROGS=\
...
	$U/_uthread\

$U/_uthread: $U/uthread.o $U/uthread_switch.o $(ULIB) $U/user.ld
	$(LD) $(LDFLAGS) -T $U/user.ld -o $U/_uthread $U/uthread.o $U/uthread_switch.o $(ULIB)
	$(OBJDUMP) -S $U/_uthread > $U/uthread.asm
```

---

## 3. Verification and Execution Trace

When executed inside xv6, the threads start, synchronize, alternate counting, and gracefully terminate:

```text
$ uthread
thread_a started
thread_b started
thread_c started
thread_c 0
thread_a 0
thread_b 0
thread_c 1
thread_a 1
thread_b 1
...
thread_c 99
thread_a 99
thread_b 99
thread_c: exit after 100
thread_a: exit after 100
thread_b: exit after 100
thread_schedule: no runnable threads
```

### Execution Lifecycle Breakdown:
1. **Startup Synchronization**:
   - `thread_a` starts, sets `a_started = 1`, and loops waiting for `b` and `c` by calling `thread_yield()`.
   - `thread_schedule` switches to `thread_b`, which sets `b_started = 1` and yields.
   - `thread_schedule` switches to `thread_c`, which sets `c_started = 1`. Because both `a_started` and `b_started` are already 1, `thread_c` completes its `while` check without yielding and prints `thread_c 0` first!
2. **Strict Interleaving**:
   - `thread_c 0` yields $\to$ `thread_a` resumes and prints `thread_a 0` $\to$ yields $\to$ `thread_b` resumes and prints `thread_b 0`.
   - The cyclic order `c -> a -> b` repeats deterministically for 100 iterations.
3. **Termination**:
   - Each thread marks its state `FREE` and invokes `thread_schedule()`. When all 3 worker threads are free, `thread_schedule` detects `next_thread == 0`, prints the sentinel message, and exits cleanly.

---

## 4. Observation Answers

### Observe Question
**Question:** What exactly does `thread_switch` save and load? Why is `ra` the register that makes execution continue in the other thread?

**Answer:**
1. **What `thread_switch` saves and loads:**
   `thread_switch(struct context *old, struct context *new)` saves the callee-saved registers of the currently running thread into `old->ctx` (passed in register `a0`) and restores the callee-saved registers of the target thread from `new->ctx` (passed in register `a1`).

   Specifically, it preserves 14 64-bit registers:
   - `ra` (offset 0): Return Address
   - `sp` (offset 8): Stack Pointer
   - `s0` (offset 16): Frame pointer / Saved register 0
   - `s1`--`s11` (offsets 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104): Saved registers 1 through 11.

   Caller-saved registers (`t0`--`t6`, `a0`--`a7`) are NOT saved by `thread_switch`. Under the standard RISC-V calling convention (ABI), a function calling `thread_yield()` or `thread_schedule()` expects caller-saved registers to be overwritten by the callee. Therefore, GCC automatically saves any live values residing in caller-saved registers onto the calling thread's own execution stack before `thread_switch` is entered.

2. **Why `ra` makes execution continue in the other thread:**
   In RISC-V assembly, the `ret` instruction is a pseudo-instruction for `jalr zero, 0(ra)`, which loads the address in register `ra` into the Program Counter (`pc`).

   When `thread_switch` executes:
   - It loads `ra` from `0(a1)` (the next thread's saved `ctx.ra`).
   - When `ret` executes:
     - **For a newly created thread**: `thread_create()` initialized `t->ctx.ra = (uint64)func`. Therefore, `ret` jumps directly to the entry point of the thread function (e.g., `thread_a`), running on its dedicated stack `sp = t->stack + STACK_SIZE`.
     - **For an existing thread returning from a yield**: `ra` holds the address of the instruction immediately following the `thread_switch()` call inside `thread_schedule()` when that thread previously yielded. Executing `ret` jumps directly to that instruction, resuming the thread seamlessly as if `thread_switch()` had just returned.

---

## 5. Conclusion

Laboratory Work No. 18 demonstrated that thread context switching is fundamentally an architectural manipulation of CPU registers (`ra`, `sp`, `s0`--`s11`) and memory stacks. By executing context switching in user space, cooperative threads achieve high efficiency with zero kernel intervention or system call overhead.


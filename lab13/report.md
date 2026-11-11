# Operating Systems -- Lab 13: Semaphores in Your Own Kernel (xv6)

**Course:** Operating Systems (Track B: build your own small OS (xv6))  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026--2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Architecture

In Labs 11 and 12, semaphores were used from Linux user space via `<semaphore.h>`. In Lab 13, counting semaphores are built directly inside the **xv6-riscv** kernel as system calls `sem_init` (`24`), `sem_down` (`25`), `sem_up` (`26`), plus the optional non-blocking `sem_trydown` (`27`).

### Two-Step Sleep Protocol in This xv6 Tree
Unlike the textbook xv6 `sleep(chan, lk)` API, this xv6 kernel splits sleeping into two calls (`kernel/proc.c`):
1. `sleep_prepare(s)` acquires `p->lock`, records `p->chan = s`, and releases `p->lock`.
2. `release(&s->lock)` drops the semaphore spinlock so `mycpu()->noff` returns to `0` and other harts can call `sem_up()`.
3. `sleep()` acquires `p->lock`, and if `p->chan != 0` (no intervening `wakeup(s)` cleared it), transitions `p->state = SLEEPING` and calls `sched()`. If `sem_up()` ran between `sleep_prepare(s)` and `sleep()`, `wakeup(s)` already cleared `p->chan = 0`, so `sleep()` returns immediately without missing the wakeup.

---

## 2. Modified and Created Files (`~/xv6-riscv`, commit `aae203a`)

- **`kernel/syscall.h`**: Added `#define SYS_sem_init 24`, `#define SYS_sem_down 25`, `#define SYS_sem_up 26`, `#define SYS_sem_trydown 27` after `SYS_trace` (`23`).
- **`user/user.h`**: Declared `int sem_init(int, int);`, `int sem_down(int);`, `int sem_up(int);`, `int sem_trydown(int);`.
- **`user/usys.pl`**: Added `entry("sem_init");`, `entry("sem_down");`, `entry("sem_up");`, `entry("sem_trydown");`.
- **`user/semtest.c`**: Test 1 checks parent-before-child ordering via semaphore `0` (`sem_init(0, 0)` + parent `pause(10)`). Test 2 checks strict `ping 1`..`3` / ` pong 1`..`3` alternation via semaphores `1` (init `1`) and `2` (init `0`).
- **`user/mix.c`**: Parent and child write 5 lines of 30 `'A'`s/`'b'`s + `'\n'` one byte per `write(1, p, 1)`; `mix lock` protects each line with semaphore `3`.
- **`user/trydown.c`** *(Optional)*: Demonstrates `sem_trydown(4)` returning `0`, `-1` when count is `0`, and `0` again after `sem_up(4)`.
- **`kernel/sem.c`**: Implements `NSEM = 8` shared semaphores (`struct sem { struct spinlock lock; int value; }`) and `sys_sem_init`, `sys_sem_down`, `sys_sem_up`, `sys_sem_trydown` using a shared `fetch_sem()` lookup helper.
- **`Makefile`**: Added `$K/sem.o \` to `OBJS` and `$U/_semtest\`, `$U/_mix\`, `$U/_trydown\` to `UPROGS`.
- **`kernel/syscall.c`**: Registered `sys_sem_init`, `sys_sem_down`, `sys_sem_up`, and `sys_sem_trydown` in `extern` prototypes, `syscalls[]`, and `syscallnames[]`.

---

## 3. QEMU Execution & Verification

### Part 1 Check (`Check 1` before wiring `kernel/sem.c`)
```text
$ semtest
3 semtest: unknown sys call 24
4 semtest: unknown sys call 25
child: I ran after the parent
parent: done, I let the child go
3 semtest: unknown sys call 26
...
```
Because the kernel did not yet implement syscalls `24..26`, `sem_down(0)` returned immediately with an unknown syscall error and the child printed before the parent finished `pause(10)`.

### Part 2 Check (`Check 2`: `semtest`)
```text
$ semtest
parent: done, I let the child go
child: I ran after the parent
ping 1
 pong 1
ping 2
 pong 2
ping 3
 pong 3
semtest: OK
```

### Part 3 Check (`mix`, `mix` again, and `mix lock`)
- Without lock (run 1 & run 2): parent and child interleave byte-by-byte across 3 CPUs (e.g., `AAAAAAAAAbAbbbbAbbbAb...`, `AAAAAbAbAbAbAb...`).
- With lock (`mix lock`):
```text
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
```

### Optional (`trydown`)
```text
$ trydown
first trydown: 0
second trydown (at 0): -1
after sem_up, third trydown: 0
```

---

## 4. Observe Answers (Copied to `notes.txt`)

1. **Without the lock, why do the letters mix? (How many system calls does one line cost?)**
   - Each call to `lines()` prints 30 letters plus `'\n'` = **31 characters**, one byte at a time via `write(1, p, 1)` inside the inner loop, so **one line costs 31 `write()` system calls**.
   - Without semaphore `3` guarding the loop, xv6 runs with 3 CPUs in QEMU (`CPUS := 3`), letting parent (`'A'`) and child (`'b'`) execute concurrently on separate harts (or preempt across syscalls) and interleave individual 1-byte `write()` calls on the console.

2. **With the lock every line is whole. Which two lines of `mix.c` make one line a critical section?**
   - `sem_down(3);` before `for (char *p = s; *p; p++)` and `sem_up(3);` after that loop (under `if (uselock)` inside `lines()`).

3. **In `sem_down` the test is `while (s->value == 0)`, not `if`. Why can a process wake up and still find `0`?**
   - `wakeup(s)` marks *all* processes sleeping on channel `s` `RUNNABLE`, not just one. The first woken process to re-acquire `s->lock` decrements `s->value` from `1` back to `0`; any other woken sleeper (or a process beaten to `s->lock` by a newly arriving caller, or woken by `kkill()`) finds `s->value == 0` upon reacquiring `s->lock` and must loop back to sleep rather than decrementing below `0`.

4. **`sem_down` calls `release(&s->lock)` BEFORE `sleep()`. What would go wrong if it slept holding the spinlock?**
   - **Immediate kernel panic (`panic("sched locks")`)**: `sleep()` acquires `p->lock` and calls `sched()` (`kernel/proc.c`), which requires `mycpu()->noff == 1` (`if (mycpu()->noff != 1) panic("sched locks");`). Holding `s->lock` and `p->lock` together sets `noff == 2` and panics immediately.
   - **Deadlock / CPU stall**: Even without that panic assertion, callers of `sem_up()` must execute `acquire(&s->lock)` to increment `s->value` and call `wakeup(s)`. If the sleeping process retained `s->lock`, no `sem_up()` caller on any CPU could ever acquire `s->lock` to wake it.


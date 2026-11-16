# Lab 13: Semaphores in Your Own Kernel (xv6)

This directory contains the complete deliverables for **Operating Systems — Lab 13 (Track B: xv6)**:

## Modified xv6 Files Included in Repository

All kernel-side and user-side files from `xv6-riscv` required to build and verify the semaphore implementation are included in this directory:

### Kernel Side (`lab13/kernel/`)
- `kernel/sem.c` (also at `lab13/sem.c`): Counting semaphores implementation (`NSEM = 8`, `struct sem { struct spinlock lock; int value; }`) and system calls `sys_sem_init()`, `sys_sem_down()`, `sys_sem_up()`, and optional non-blocking `sys_sem_trydown()`.
- `kernel/syscall.h`: Defined `SYS_sem_init 24`, `SYS_sem_down 25`, `SYS_sem_up 26`, and `SYS_sem_trydown 27`.
- `kernel/syscall.c`: Added extern prototypes, registered entries in `syscalls[]` table, and added names to `syscallnames[]`.

### User Side (`lab13/user/`)
- `user/user.h`: Prototypes for `int sem_init(int, int);`, `int sem_down(int);`, `int sem_up(int);`, and `int sem_trydown(int);`.
- `user/usys.pl`: Syscall stubs for `entry("sem_init")`, `entry("sem_down")`, `entry("sem_up")`, and `entry("sem_trydown")`.
- `user/semtest.c` (also at `lab13/semtest.c`): Test program verifying parent-before-child ordering (`sem 0`) and strict 3-round ping-pong alternation (`sem 1` & `sem 2`).
- `user/mix.c` (also at `lab13/mix.c`): Multiprocess console printer demonstrating character mixing without locks and whole-line preservation under `mix lock` via semaphore 3.
- `user/trydown.c` (also at `lab13/trydown.c`): Verifies `sem_trydown(4)` returning 0, -1 at 0, and 0 after `sem_up(4)`.
- `Makefile`: Configured with `$K/sem.o \` in `OBJS` and `$U/_semtest\`, `$U/_mix\`, `$U/_trydown\` in `UPROGS`.

## Unified Git Patch
- `sem.patch`: Unified diff patch against xv6 with `trace`. To apply to an `xv6-riscv` checkout:
  ```bash
  cd ~/xv6-riscv
  git apply /path/to/lab13/sem.patch
  make qemu
  ```

## Reports and Verification Outputs
- `report.md`: Markdown report detailing design, implementation, and analysis.
- `report.tex` / `report.pdf` / `Lab13_IulianPavlov_FAF-241.pdf`: Formatted LaTeX report.
- `results.txt`: Console outputs from `semtest`, `mix`, `mix lock`, and `trydown`.
- `notes.txt`: Answers to the four observation questions from Part 3.

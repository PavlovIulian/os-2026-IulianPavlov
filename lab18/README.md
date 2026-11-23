# Lab 18: Build a User-Level Thread Switch (Track B: xv6)

This directory contains the complete deliverables for **Operating Systems -- Lab 18 (Track B: xv6)**:

## Modified and Created xv6 Files Included

All files required to build, link, and run user-space cooperative threading (`uthread`) in xv6-riscv are provided:

### User-Level Threading Implementation
- `user/uthread.c` (also mirrored at `uthread.c`):
  - `struct context`: Callee-saved register state (`ra`, `sp`, `s0`--`s11`, 14 64-bit integer fields).
  - `thread_create(void (*func)())`: Allocates a `struct thread`, marks `state = RUNNABLE`, initializes `t->ctx.ra = (uint64)func`, and aligns `t->ctx.sp = (uint64)t->stack + STACK_SIZE`.
  - `thread_schedule()`: Round-robin search over `all_thread[MAX_THREAD]`; on thread switch, updates state, saves old thread, and executes `thread_switch(&old->ctx, &next_thread->ctx)`.
  - `thread_b()` and `thread_c()`: Cooperative worker functions waiting for synchronization flags, interleaving counts $0..99$ via `thread_yield()`, and exiting cleanly.
- `user/uthread_switch.S` (also mirrored at `uthread_switch.S`):
  - RISC-V assembly routine saving callee-saved registers of `old` (`a0`), restoring callee-saved registers of `new` (`a1`), and executing `ret` into the new thread.
- `Makefile`:
  - Added `$U/_uthread\` to `UPROGS`.
  - Appended explicit multi-object link rule for `$U/_uthread` linking both `uthread.o` and `uthread_switch.o` with `$(ULIB)`.

## Unified Git Patch
- `uthread.patch`: Unified diff patch against xv6. To apply to an `xv6-riscv` tree:
  ```bash
  cd ~/xv6-riscv
  git apply /path/to/lab18/uthread.patch
  make qemu
  ```
  Inside xv6 shell:
  ```text
  $ uthread
  ```

## Reports and Verification Outputs
- `report.md`: Markdown technical report covering theoretical architecture, assembly analysis, and scheduling mechanics.
- `report.tex` / `report.pdf` / `Lab18_IulianPavlov_FAF-241.pdf`: Formatted IEEE/academic PDF report compiled via Tectonic.
- `notes.txt`: Explicit answers to the Observe questions regarding register saving and the role of `ra`.
- `results.txt`: Full console output log demonstrating interleaved counting and graceful termination.
- `logo.png`: UTM institutional crest.


# Lab 10: Add a System Call (`trace`) in xv6

This folder contains the complete deliverables for **Operating Systems — Lab 10 (Track B: xv6)**:

## Modified xv6 Files Included in Repository

As required, all modified kernel-side and user-side files from `xv6-riscv` are included in this directory:

### Kernel Side (`lab10/kernel/`)
- `kernel/syscall.h`: Defined `SYS_trace 23` (following `SYS_sync 22`).
- `kernel/syscall.c`: Registered `extern uint64 sys_trace(void);`, entry `[SYS_trace] = sys_trace` in `syscalls[]`, populated `syscallnames[]`, and added logging via `printk()` in `syscall()` when `(p->trace_mask >> num) & 1`.
- `kernel/sysproc.c`: Implemented `sys_trace()` handler storing `mask` from `argint(0, &mask)` into `myproc()->trace_mask`.
- `kernel/proc.h`: Added `int trace_mask;` to `struct proc` to track the active trace bitmask per process.
- `kernel/proc.c`: In `fork()` / `kfork()`, propagated the mask to child processes: `np->trace_mask = p->trace_mask;`.

### User Side (`lab10/user/` & Root)
- `user/user.h`: Declared `int trace(int);` prototype.
- `user/usys.pl`: Added `entry("trace");` to generate assembly stub with `li a7, SYS_trace; ecall; ret`.
- `user/trace.c` (also present as `lab10/trace.c`): User CLI wrapper that parses `argv[1]`, invokes `trace(mask)`, and `exec()`s the requested command.
- `Makefile`: Added `$U/_trace\` to `UPROGS` in xv6 build manifest.

## Unified Git Patch
- `trace.patch`: Unified diff patch against base xv6. To apply to an unmodified `xv6-riscv` checkout:
  ```bash
  cd ~/xv6-riscv
  git apply /path/to/lab10/trace.patch
  make qemu
  ```

## Reports and Verification Outputs
- `report.md`: Complete markdown report with full theoretical explanations, code listings, and verification.
- `report.tex` / `report.pdf` / `Lab10_IulianPavlov_FAF-241.pdf`: Formatted LaTeX report.
- `results.txt`: Raw console logs from QEMU verification runs.
- `notes.txt`: Detailed answers to all observation questions from Part 3.

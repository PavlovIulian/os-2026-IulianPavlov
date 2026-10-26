# Laboratory Work No. 2: Meet the OS You Will Build (xv6)

**Course:** Operating Systems (Track B: xv6)  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Overview and Purpose
The goal of this laboratory is to establish a working environment for the xv6 operating system, compile the kernel and userland programs from source, run it within the QEMU emulator, analyze its architectural organization, and develop a first custom user-space utility (`sleep`).

Key learning objectives include:
- Setting up the cross-compilation toolchain for the 64-bit RISC-V (`riscv64`) target architecture.
- Compiling the kernel, user-space utilities, and filesystem image (`fs.img`) from scratch.
- Booting the compiled operating system under the QEMU machine emulator (`qemu-system-riscv64`).
- Interacting with the minimalist xv6 Unix userland (file manipulation, pipes, redirection).
- Investigating the fundamental architectural boundary between user-space code (`user/`) and kernel-space code (`kernel/`).
- Understanding how system calls bridge the hardware privilege barrier between User mode (U-mode) and Supervisor mode (S-mode).
- Writing, building, and verifying a new user program (`user/sleep.c`) that interacts with the kernel timer subsystem.

### 1.2 Theoretical Foundations: xv6 and RISC-V Architecture
xv6 is a modern re-implementation of Dennis Ritchie and Ken Thompson's Sixth Edition Unix (v6, 1975), rewritten in clean ANSI C for multi-core RISC-V processors. Unlike massive modern kernels such as Linux (~30 million lines of code), xv6 is approximately 9,000 lines of code, making it small enough to hold the entire operating system architecture in memory.

#### Hardware Privilege Levels
The RISC-V architecture enforces hardware protection through three privilege modes:
1. **Machine Mode (M-mode):** Highest privilege level with unrestricted access to physical hardware. Used primarily during initial hardware boot (bootloader).
2. **Supervisor Mode (S-mode):** Operating system kernel privilege level. In xv6, the kernel executes in S-mode, allowing it to configure page tables (`satp`), manage interrupts, and control peripherals.
3. **User Mode (U-mode):** Unprivileged mode where application programs (`sh`, `ls`, `cat`, `sleep`) run. User programs cannot directly access physical hardware or modify memory outside their allocated virtual address space.

#### System Call Transition Mechanism
When a user-space program requires a privileged kernel service (e.g., reading a file or sleeping for timer ticks), it cannot directly jump into kernel memory. Instead, it triggers a hardware trap using the RISC-V `ecall` (environment call) instruction. The hardware switches the CPU to S-mode and vectors execution to a designated trap handler in `kernel/trampoline.S`, which preserves user registers, validates arguments, invokes the corresponding kernel function, and returns control to user space via `sret`.

---

## 2. Environment Setup and Toolchain Configuration

### 2.1 Cloning the Repository
The xv6 source code was obtained from the upstream MIT repository:
```bash
cd ~/xv6-riscv 2>/dev/null || \
git clone https://github.com/mit-pdos/xv6-riscv ~/xv6-riscv
cd ~/xv6-riscv
ls
```

**Terminal Output:**
```
Cloning into '/home/sunflower/xv6-riscv'...
remote: Enumerating objects: 8334, done.
remote: Counting objects: 100% (33/33), done.
remote: Compressing objects: 100% (22/22), done.
Receiving objects: 100% (8334/8334), 17.67 MiB | 2.23 MiB/s, done.
Resolving deltas: 100% (4583/4583), done.
Permissions Size User      Date Modified Name
drwxr-xr-x     - sunflower 28 Sep 17:30  kernel
drwxr-xr-x     - sunflower 28 Sep 17:30  mkfs
drwxr-xr-x     - sunflower 28 Sep 17:30  user
.rw-r--r--  1.2k sunflower 28 Sep 17:30  LICENSE
.rw-r--r--  5.7k sunflower 28 Sep 17:30  Makefile
.rw-r--r--  2.4k sunflower 28 Sep 17:30  README
.rwxr-xr-x  6.1k sunflower 28 Sep 17:30  test-xv6.py
```

### 2.2 Toolchain Diagnostics and Installation on Arch Linux
An initial build attempt confirmed that the cross-compilation toolchain and emulator were absent:
```bash
make qemu
```
**Diagnostic Error:**
```
*** Error: Couldn't find a riscv64 version of GCC/binutils.
*** To turn off this error, run 'gmake TOOLPREFIX= ...'.
/bin/sh: line 1: qemu-system-riscv64: command not found
/bin/sh: line 1: bc: command not found
gcc -march=rv64gc -g -ffile-prefix-map=/home/sunflower/xv6-riscv=. -c -o kernel/entry.o kernel/entry.S
cc1: error: bad value 'rv64gc' for '-march=' switch
make: *** [Makefile:99: kernel/entry.o] Error 1
```

The necessary tools were installed via Arch Linux's package manager `pacman`:
```bash
sudo pacman -S --needed base-devel git bc qemu-system-riscv \
  riscv64-elf-gcc riscv64-elf-binutils riscv64-elf-gdb
```

This installed the bare-metal RISC-V cross-compiler (`riscv64-elf-gcc`), linker/assembler (`riscv64-elf-binutils`), debugger, and QEMU RISC-V system emulator (`qemu-system-riscv64`).

---

## 3. Part 1: Build and Boot xv6

With the toolchain installed, the source tree was cleaned and compiled:
```bash
cd ~/xv6-riscv
make clean
make qemu
```

### 3.1 QEMU Launch and Boot Log
```
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 \
  -nographic -global virtio-mmio.force-legacy=false -drive file=fs.img,if=none,format=raw,id=x0 \
  -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$
```

### 3.2 Boot Analysis
1. **Emulated Machine:** QEMU models a RISC-V `virt` board with 128 MB RAM, 3 symmetrical cores (harts), and a VirtIO block storage device mapped to `fs.img`.
2. **Multicore Initialization:** Hart 0 boots first, performs kernel setup, and starts Hart 1 and Hart 2 (`hart 2 starting`, `hart 1 starting`).
3. **Userland Hand-off:** The kernel initializes `initcode.S`, which launches the first user process `init` (`init.c`). In turn, `init` spawns the user shell (`user/sh.c`), displaying the `$` prompt.

---

## 4. Part 2: Operating xv6 as a Unix System

Inside the running xv6 shell, standard utilities and shell pipelines were tested.

### 4.1 Listing Files (`$ ls`)
```
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2441
cat            2 3 38112
echo           2 4 36888
forktest       2 5 18248
grep           2 6 41648
init           2 7 37272
kill           2 8 13240
ln             2 9 12344
ls             2 10 16248
mkdir          2 11 13320
rm             2 12 13304
sh             2 13 42352
stressfs       2 14 15304
usertests      2 15 152000
wc             2 16 14240
zombie         2 17 12200
console        3 18 0
```
*Columns:* Name, Type (1 = directory, 2 = regular file, 3 = device), Inode number, Size in bytes.

### 4.2 Displaying File Contents (`$ cat README`)
```
$ cat README
xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
but is implemented for a modern RISC-V multiprocessor using ANSI C.
...
```

### 4.3 Outputting Text (`$ echo hello xv6`)
```
$ echo hello xv6
hello xv6
```

### 4.4 Inter-Process Pipeline (`$ ls | grep c`)
```
$ ls | grep c
cat            2 3 38112
echo           2 4 36888
wc             2 16 14240
console        3 18 0
```
The shell constructed a pipeline where the standard output of `ls` was redirected to the standard input of `grep`. Only entries containing the letter `c` were matched and displayed.

### 4.5 Counting Lines, Words, and Bytes (`$ wc README`)
```
$ wc README
48 336 2441 README
```
`README` contains 48 lines, 336 words, and 2441 bytes.

### 4.6 Kernel Regression Test Suite (`$ usertests -q`)
To verify kernel integrity across memory management, trap handling, file operations, and concurrency, the comprehensive `usertests` suite was executed:
```
$ usertests -q
usertests starting
test copyin: OK
test copyout: OK
...
test lazy_copy: OK
test lazy_copyinstr: OK
test lazy_sbrk: OK
test partial_write: OK
test unlinkcwd: OK
ALL TESTS PASSED
```
All system tests passed successfully, confirming kernel stability and correct memory isolation.

---

## 5. Part 3: Reading the Source Tree

### 5.1 Structure of a User Program
Examining `user/ls.c` (lines 1–40):
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  buf[sizeof(buf)-1] = '\0';
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
...
```

Examining `user/cat.c` (lines 1–30):
```c
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];

void
cat(int fd)
{
  int n;

  while((n = read(fd, buf, sizeof(buf))) > 0) {
    if (write(1, buf, n) != n) {
      fprintf(2, "cat: write error\n");
      exit(1);
    }
  }
  if(n < 0){
    fprintf(2, "cat: read error\n");
    exit(1);
  }
}
```

### 5.2 The System Call Boundary: `user/user.h` and `kernel/sysfile.c`
In `user/cat.c`, functions like `read()`, `write()`, and `exit()` are declared in `user/user.h`:
```c
// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sys_sbrk(int, int);
int pause(int);
int uptime(void);
int sync(void);
```

Searching for the kernel-side handlers in `kernel/sysfile.c`:
```bash
grep -n "sys_read\|sys_write" kernel/sysfile.c | head
```
**Output:**
```
69:sys_read(void)
83:sys_write(void)
```

**Tracing Flow:**
1. The user program invokes `read(fd, buf, n)`.
2. Execution enters the assembly stub in `user/usys.S`:
   ```assembly
   .global read
   read:
    li a7, SYS_read
    ecall
    ret
   ```
3. `ecall` elevates CPU privilege to S-mode and enters `kernel/trap.c`.
4. The dispatcher `syscall()` in `kernel/syscall.c` looks up `syscalls[SYS_read]` which points to `sys_read()` in `kernel/sysfile.c`.
5. `sys_read()` extracts arguments from the trapframe using `argfd()` and `argaddr()`, executes file I/O, and stores the byte count in trapframe `a0`.
6. Return instruction `sret` restores U-mode and returns the result to user space.

---

## 6. Part 4: Implementation of `user/sleep.c`

### 6.1 Source Code (`user/sleep.c`)
```c
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "usage: sleep <ticks>\n");
    exit(1);
  }

  // Note: Upstream MIT xv6 renamed sleep() to pause() in Aug 2025
  // to avoid name collisions with the internal kernel synchronization primitive.
  pause(atoi(argv[1]));
  exit(0);
}
```

### 6.2 Makefile Integration
The program was registered in the xv6 `Makefile` under `UPROGS`:
```bash
sed -i 's|^\t$U/_echo\\$|&\n\t$U/_sleep\\|' Makefile
grep -n "_echo\|_sleep" Makefile
```
**Output:**
```
134:	$U/_echo\
135:	$U/_sleep\
```

### 6.3 Functional Verification
The system was rebuilt and booted under QEMU:
```bash
make qemu
```
Inside the xv6 shell:
```
$ sleep 10
$ sleep
usage: sleep <ticks>
```
- **`sleep 10`**: Execution successfully paused for 10 clock ticks (~1 second) before returning to the prompt.
- **`sleep` (no arguments)**: Caught by `argc != 2`, printed the error message to stderr (fd 2), and exited with code 1.

---

## 7. Answers to Laboratory Questions and Observations

### 7.1 Part 2 Observe Questions
1. **List three programs xv6 ships with.**
   - `cat`: Reads files sequentially and writes them to standard output.
   - `echo`: Echoes command-line arguments to stdout.
   - `grep`: Filters input lines matching a specified string or pattern.
   *(Other utilities include `ls`, `wc`, `rm`, `mkdir`, `kill`, `ln`, `sh`).*

2. **You just used a pipe (`|`) inside xv6. Which two OS features must exist for a pipe to work?**
   - **Feature 1: Process Concurrency and Isolation (`fork()`):** The operating system must support instantiating and concurrently running multiple isolated processes with independent memory spaces. In `ls | grep c`, both `ls` and `grep` run concurrently.
   - **Feature 2: Inter-Process Communication (IPC) via Kernel Buffering and File Descriptors (`pipe()` and `dup()`):** The kernel must provide a synchronized FIFO ring-buffer in memory, addressable via file descriptors. The shell attaches the write descriptor to stdout of the producer (`ls`) and the read descriptor to stdin of the consumer (`grep`). The kernel scheduler handles coordination (blocking the consumer when empty, blocking the producer when full).

3. **In one sentence: how does the xv6 shell compare to the Linux shell from Lab 1?**
   > *The xv6 shell is a compact, ~500-line educational command interpreter implementing only fundamental POSIX primitives (`fork`, `exec`, `pipe`, `<`, `>`, `;`, `&`, `cd`), whereas the Linux GNU Bash shell is a feature-rich production shell offering globbing (`*`), environment variables, tab autocompletion, job control, and command history.*

### 7.2 Part 3 Source-Reading and "The Big Idea" Answers
1. **Explain the split between `kernel/` and `user/`.**
   - `kernel/` represents the privileged core operating system executing in RISC-V Supervisor mode (S-mode). It has direct access to physical memory, page tables, hardware control registers, and device interrupts. It provides process scheduling, virtual memory paging, and filesystem management.
   - `user/` holds unprivileged programs running in User mode (U-mode). User programs operate within isolated virtual address spaces and cannot directly issue privileged CPU instructions or access raw hardware.

2. **Why are `read()` and `write()` in `user/cat.c` not ordinary C functions?**
   - Ordinary C functions execute purely within the calling process's user-space stack frame.
   - `read()` and `write()` are system calls: they invoke an assembly stub (`usys.S`) that places a syscall identifier into CPU register `a7` and executes the `ecall` instruction.
   - This hardware trap forces a privilege transition into S-mode, redirecting CPU control to kernel-space handlers (`sys_read`, `sys_write` in `kernel/sysfile.c`).

3. **The Big Idea Reflection:**
   - In Linux (Lab 1), system interactions are observed only from the outside through POSIX APIs, wrappers, and utilities. In xv6, because the code is concise (~9,000 lines of ANSI C), we can inspect and modify **both** sides of the interface: the user program requesting service (`user/`) and the kernel routine responding to it (`kernel/`).

---

## 8. Conclusions
1. **Toolchain Setup:** Configured a complete bare-metal RISC-V cross-compilation toolchain on Arch Linux and successfully compiled and booted xv6 under QEMU.
2. **Userland and Kernel Architecture:** Investigated the clear hardware-enforced separation between User mode applications and Supervisor mode kernel routines.
3. **System Call Lifecycle:** Traced the complete lifecycle of a system call from user-space assembly stubs (`ecall`) through trap vectors into kernel handlers.
4. **Hands-on Development:** Successfully implemented the `sleep` utility, updated the build configuration in `Makefile`, and validated tick-based timing and argument parsing within xv6.

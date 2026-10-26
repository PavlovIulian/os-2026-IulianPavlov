# Laboratory Work No. 8: The Same Calls, Inside Your Own OS

**Course:** Operating Systems (Track B: build your own small OS (xv6))  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Laboratory Objectives
In Laboratory Work No. 7, the core process management primitives (`fork`, `exec`, and `wait`) were explored within a standard Linux user-space environment. In Laboratory Work No. 8, we transition these exact fundamental concepts into xv6—the teaching operating system built from source on the RISC-V architecture. 

Inside xv6, there is no glibc, no `/bin/bash`, and no dynamic linking: only the bare-metal kernel system calls declared directly in `user/user.h`. The objectives of this laboratory are:
- **Process Lifecycle in xv6 (`run.c`):** Implementing the classic shell sequence (`fork()`, `exec()`, `wait()`) to execute user binaries (`echo`, `ls`), capturing exit statuses, and observing execution failure paths.
- **Comparing Linux vs. xv6 Primitives:** Analyzing the direct parallels between POSIX system calls on Linux and native xv6 system calls.
- **Bidirectional Pipe Communication (`pingpong.c`):** Coordinating two processes via two distinct anonymous pipes to exchange a byte back and forth without data races, while ensuring all unused pipe descriptors are cleanly closed.
- **Concurrent Streaming Pipeline (`primes.c` - Optional):** Implementing Doug McIlroy's classic concurrent prime sieve design, chaining independent filter processes connected via anonymous pipes.
- **Identifying Kernel Resource Bottlenecks:** Exploring system-wide open file table limits (`NFILE = 100`) and per-process descriptor limits (`NOFILE = 16`).
- **Command Argument Orchestration (`xargs.c` - Stretch Goal):** Implementing the UNIX `xargs` utility for xv6, parsing standard input line-by-line and executing target binaries with combined argument lists.

### 1.2 Core Architectural Primitives in xv6
1. **Process Forking and Image Replacement:**
   - `fork()` duplicates the calling process, copying the user memory page tables and duplicating open file descriptors.
   - `exec(char *path, char **argv)` reads an ELF binary from the filesystem, creates a fresh user address space, loads code and data, and sets the program counter to the ELF entry point.
   - `wait(int *status)` blocks until a child process terminates, freeing the child's process table slot and returning its exit status code.
2. **Pipes and Ring Buffers:** In xv6 (`kernel/pipe.c`), an anonymous pipe is represented by `struct pipe`, comprising a 512-byte circular buffer (`char data[PIPESIZE]`), read/write byte offsets (`nread`, `nwrite`), and boolean flags indicating whether the read or write ends are open (`readopen`, `writeopen`). Access is synchronized with a spinlock.
3. **Blocking and Waking:** When a process reads from an empty pipe, the kernel calls `sleep(&pi->nread, &pi->lock)` to yield the CPU. When another process issues a `write()`, it invokes `wakeup(&pi->nread)`, moving suspended readers back to the `RUNNABLE` state.
4. **End-of-File (EOF) Semantics:** A reader receives an EOF (`read()` returns `0`) if and only if the pipe buffer is completely empty **and** `pi->writeopen == 0`. If even a single process in the entire operating system retains an open descriptor to the write end, readers will block indefinitely waiting for future data.

---

## 2. Part 1: Process Lifecycle inside xv6 (`run.c`)

### 2.1 Implementation Details
We developed `user/run.c` to replicate the fundamental three steps of a shell directly inside xv6:
1. `fork()` to create a child process.
2. `exec()` inside the child process to become `echo`.
3. `wait()` inside the parent process to wait for the child and retrieve its exit status.

```c
#include "kernel/types.h"
#include "user/user.h"

// the three steps of the shell, inside xv6: echo hello from the child
int
main(int argc, char *argv[])
{
  printf("parent: my pid is %d\n", getpid());

  int pid = fork(); // step 1: copy myself
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }

  if(pid == 0){ // only the child runs this
    printf("child: my pid is %d\n", getpid());
    char *args[] = { "echo", "hello", "from", "the", "child", 0 };
    exec("echo", args);
    printf("exec failed\n");
    exit(1);
  }

  int status;
  wait(&status);
  printf("parent: child %d ended with status %d\n", pid, status);
  exit(0);
}
```

### 2.2 Compilation and Execution Trace
The utility was registered in the xv6 `Makefile` under `UPROGS`:
```makefile
UPROGS=\
    $U/_cat\
    ...
    $U/_run\
    $U/_pingpong\
    ...
```

Executing under QEMU (`make qemu`):
```bash
$ run
parent: my pid is 3
child: my pid is 4
hello from the child
parent: child 4 ended with status 0
$
```

### 2.3 Variant: Child Executes `ls`
To demonstrate that `exec()` can invoke any executable binary on the filesystem, we modified the child block in `user/run.c`:
```c
char *args[] = { "ls", 0 };
exec("ls", args); // step 2: become ls
```

Rebuilding and executing in QEMU:
```bash
$ run
parent: my pid is 5
child: my pid is 6
.              1 1 1024
..             1 1 1024
README         2 2 2441
cat            2 3 38112
echo           2 4 36888
run            2 5 37120
...
parent: child 6 ended with status 0
$
```

### 2.4 Variant: Handling Execution Failure (`nosuchprog`)
To test the failure branch of `exec()`, we attempted to execute a non-existent binary:
```c
char *args[] = { "nosuchprog", 0 };
exec("nosuchprog", args);
printf("exec failed\n");
exit(1);
```

When `exec()` fails (because `nosuchprog` does not exist in the filesystem), it returns $-1$ without replacing process memory, causing execution to continue directly to `printf("exec failed\n")` and `exit(1)`.

Executing in QEMU:
```bash
$ run
parent: my pid is 7
child: my pid is 8
exec failed
parent: child 8 ended with status 1
$
```

### 2.5 Answers to Questions (Observe — Part 1)

#### (1) Write the three calls that `run.c` uses in xv6 and the three calls `run.c` used on Linux in Lab 7. What is the same?
- **Calls in xv6 (`run.c`):**
  1. `fork()`
  2. `exec()`
  3. `wait(&status)`
- **Calls on Linux in Lab 7 (`myrun.c`):**
  1. `fork()`
  2. `execvp()` (or `execve()`)
  3. `waitpid(pid, &status, 0)` (or `wait(&status)`)
- **What is the same:**  
  The core UNIX process execution model and call semantics are identical:
  - `fork()` clones the caller into parent and child processes with separate address spaces.
  - `exec()` loads a new program into the calling process, replacing its memory image.
  - `wait()` suspends the parent process until the child terminates, reclaiming its process table entry and reading its exit status.

#### (2) Who prints "hello from the child": the code of `run`, or `echo`?
**`echo`** prints it.  
When `exec("echo", args)` succeeds, the memory space of `run` in the child process is completely replaced by the `/echo` executable. The instructions following `exec()` in `run.c` are wiped out and never executed. Control begins at `main()` in `echo.c`, which iterates through `argv` and prints `"hello from the child"`.

#### (3) Why is the status 1 here and 127 on Linux?
The exit status collected by `wait(&status)` is determined by the integer value passed to `exit()` in the child's error-handling block:
- In **xv6 (`run.c`)**, the failure path explicitly calls `exit(1)`:
  ```c
  printf("exec failed\n");
  exit(1); // Exits with code 1
  ```
- On **Linux in Lab 7 (`myrun.c`)**, the failure path explicitly called `exit(127)`:
  ```c
  perror("execvp");
  exit(127); // Standard POSIX shell exit code for "command not found"
  ```
Because `wait(&status)` directly reads the status passed to `exit()`, xv6 yields status `1`, while Linux yields status `127`.

---

## 3. Part 2: Inter-Process Communication via Pipes (`pingpong.c`)

### 3.1 Implementation Details
We developed `user/pingpong.c`. The parent process sends a single byte to the child through pipe `p2c` (parent-to-child). The child reads the byte, prints `<pid>: received ping`, sends the byte back through pipe `c2p` (child-to-parent), and exits. The parent reads the byte from `c2p`, prints `<pid>: received pong`, and reaps the child via `wait(0)`.

```c
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p2c[2], c2p[2];
  char b = 'x';

  if(pipe(p2c) < 0 || pipe(c2p) < 0){
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }
  if(fork() == 0){
    close(p2c[1]); /* child does not write to p2c */
    close(c2p[0]); /* child does not read from c2p */
    if(read(p2c[0], &b, 1) != 1)
      exit(1);
    printf("%d: received ping\n", getpid());
    write(c2p[1], &b, 1);
    close(p2c[0]);
    close(c2p[1]);
    exit(0);
  }
  close(p2c[0]); /* parent does not read from p2c */
  close(c2p[1]); /* parent does not write to c2p */
  write(p2c[1], &b, 1);
  read(c2p[0], &b, 1);
  printf("%d: received pong\n", getpid());
  close(p2c[1]);
  close(c2p[0]);
  wait(0);
  exit(0);
}
```

### 3.2 Compilation and Execution Trace
The utility was registered in the xv6 `Makefile` under `UPROGS`:
```makefile
UPROGS=\
    $U/_cat\
    ...
    $U/_zombie\
    $U/_run\
    $U/_pingpong\
    $U/_primes\
    $U/_xargs\
```

We launched xv6 under QEMU and verified execution:
```bash
$ pingpong
4: received ping
3: received pong
```
As mandated by the protocol, the child's `"received ping"` output arrives strictly before the parent's `"received pong"`.

### 3.3 Answers to Questions (Observe — Part 2)

#### (1) Why do you need TWO pipes, and not one shared by both directions?
Pipes in UNIX and xv6 are strictly **unidirectional** byte streams. A pipe has a single shared kernel ring buffer. If a single pipe were used for both directions:
- When the parent writes the byte `'x'` into the pipe and immediately executes `read()`, the parent would race against the child and would almost certainly read back its own byte from the buffer before the child was ever scheduled.
- Sharing a single pipe creates severe race conditions, non-deterministic self-consumption of messages, and potential deadlocks. 
Two separate pipes—`p2c` for parent-to-child data and `c2p` for child-to-parent data—provide isolated, unidirectional channels with independent ring buffers, ensuring strict ping-pong ordering without race conditions.

#### (2) Nothing in your code says "wait until the child has replied". What makes the parent wait?
The blocking synchronization semantics of the **`read()`** system call force the parent to wait. When the parent calls:
```c
read(c2p[0], &b, 1);
```
the child has not yet executed its `write()`. Because the ring buffer of pipe `c2p` is empty and an open write descriptor (`c2p[1]`) exists in the child, the xv6 kernel suspends the parent process inside `kernel/pipe.c`:
```c
while(pi->nread == pi->nwrite && pi->writeopen){
    ...
    sleep(&pi->nread, &pi->lock);
}
```
The parent remains asleep until the child writes the response byte and calls `wakeup(&pi->nread)`. Only then does the parent awaken, read the byte, and proceed.

#### (3) Why does the child print a larger PID than the parent?
In xv6, process identifiers (PIDs) are assigned using a monotonically increasing global counter in `kernel/proc.c`:
```c
int
allocpid()
{
  int pid;
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);
  return pid;
}
```
The parent process was allocated its PID (PID 3) when it was spawned by the shell. When the parent later calls `fork()`, the kernel invokes `allocpid()` to assign a PID to the newly created child process, which takes the next integer in sequence (PID 4). Consequently, the child's PID is always strictly greater than that of its parent.

---

## 4. Part 3: Concurrent Prime Sieve (`primes.c` - Optional)

### 4.1 Design of the Concurrent Sieve
Doug McIlroy (inventor of the UNIX pipe) conceived the concurrent prime sieve:
- A generating process feeds a stream of consecutive integers ($2, 3, \dots, 35$) into an initial pipe.
- Each filter stage reads its first incoming number $p$. By definition of the sieve, $p$ is prime, so the stage immediately prints `prime p`.
- The stage then creates an outgoing pipe, forks a child process to serve as the next stage, and forwards through the pipe only those subsequent numbers $n$ that are **not** divisible by $p$ ($n \pmod p \neq 0$).
- When the incoming pipe reaches EOF (`read() == 0`), the stage closes its outgoing pipe, waits for its child to exit, and terminates.

### 4.2 Implementation of `user/primes.c`
To avoid infinite recursion warnings under `-Werror`, `stage()` is declared with `__attribute__((noreturn))`:

```c
#include "kernel/types.h"
#include "user/user.h"

#define LIMIT 35

void stage(int) __attribute__((noreturn));

void
stage(int in)
{
  int p, n, fd[2], pid;

  /* First number read is prime */
  if(read(in, &p, sizeof(p)) != sizeof(p)){
    close(in);
    exit(0);
  }
  printf("prime %d\n", p);

  /* Allocate pipeline to next stage */
  if(pipe(fd) < 0){
    printf("primes: pipe failed after prime %d\n", p);
    close(in);
    exit(1);
  }
  pid = fork();
  if(pid < 0){
    printf("primes: fork failed after prime %d\n", p);
    close(fd[0]); close(fd[1]); close(in);
    exit(1);
  }
  if(pid == 0){
    close(fd[1]); /* Child reads from fd[0] */
    close(in);    /* Child does not need parent's input pipe */
    stage(fd[0]);
  }
  close(fd[0]);   /* Parent only writes to fd[1] */
  while(read(in, &n, sizeof(n)) == sizeof(n)){
    if(n % p != 0)
      write(fd[1], &n, sizeof(n));
  }
  close(in);
  close(fd[1]);   /* Closes write end to deliver EOF to next stage */
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int fd[2];

  if(pipe(fd) < 0)
    exit(1);
  if(fork() == 0){
    close(fd[1]);
    stage(fd[0]);
  }
  close(fd[0]);
  for(int i = 2; i <= LIMIT; i++)
    write(fd[1], &i, sizeof(i));
  close(fd[1]);
  wait(0);
  exit(0);
}
```

### 4.3 Sieve Pipeline Architecture
```
[Numbers 2..35]
      |
      v
 [Stage p=2]  == (Filters multiples of 2: 4, 6, 8, ...)
      |
    (3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35)
      v
 [Stage p=3]  == (Filters multiples of 3: 9, 15, 21, 27, 33)
      |
    (5, 7, 11, 13, 17, 19, 23, 25, 29, 31, 35)
      v
 [Stage p=5]  == (Filters multiples of 5: 25, 35)
      |
    (7, 11, 13, 17, 19, 23, 29, 31)
      v
 [Stage p=7]  == (Filters multiples of 7: none remaining <= 35)
      |
      v
     ...
      |
      v
 [Stage p=31] == (Prints prime 31; pipe closes; EOF cascades)
```

### 4.4 Execution Trace
```bash
$ primes
prime 2
prime 3
prime 5
prime 7
prime 11
prime 13
prime 17
prime 19
prime 23
prime 29
prime 31
```
All primes between 2 and 35 are discovered and printed in sequential ascending order.

### 4.5 Answers to Sieve Questions
1. **How many processes exist at the moment "prime 31" is printed?**  
   There are **12 processes** simultaneously alive: 1 main generator process + 11 filter stages ($p \in \{2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31\}$).
2. **Limit 280 Bottleneck:**  
   Changing the limit to 280 fails at prime 227 (`primes: pipe failed after prime 227`). This hits `NFILE = 100` (system-wide open file table limit in `kernel/param.h`), because each active pipe requires two file table slots ($49 \times 2 = 98$ slots plus standard descriptors).
3. **Why is this a parallel sieve?**  
   Each prime filter stage executes as an independent concurrent process that reads from its input pipe and streams non-multiples into its output pipe as soon as numbers arrive, rather than waiting for preceding stages to finish filtering the entire dataset. Multiple candidate numbers at different stages of the sieve are processed simultaneously across pipeline stages.

---

## 5. Part 4: Stretch Assignment — Implementing `xargs` (`xargs.c`)

### 5.1 Specification and Design
The `xargs` utility reads arguments from standard input separated by newlines and executes a specified command once per line, appending the arguments from the input line to the end of the provided command arguments:
- In xv6 there is no `$PATH` environment variable, so `exec()` is called with the exact binary name provided.
- Arguments on an input line can be separated by spaces, which must be split into distinct argument strings.
- For each input line, `xargs` forks a child process, executes the command with `exec()`, and waits for the child to exit before reading the next line.

### 5.2 Implementation of `user/xargs.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[512], *args[MAXARG], *s;
  int n, r, k, i;
  char c;

  /* Copy command and initial arguments */
  for(i = 1; i < argc && i < MAXARG - 1; i++)
    args[i-1] = argv[i];
  int base = i - 1;

  for(;;){
    n = 0;
    /* Read one line from stdin */
    while((r = read(0, &c, 1)) == 1 && c != '\n'){
      if(n < sizeof(buf) - 1)
        buf[n++] = c;
    }
    if(r != 1 && n == 0)
      break;
    buf[n] = 0;

    /* Parse space-separated arguments from the line */
    k = base;
    s = buf;
    while(*s && k < MAXARG - 1){
      while(*s == ' ') *s++ = 0;
      if(*s == 0) break;
      args[k++] = s;
      while(*s && *s != ' ') s++;
    }
    args[k] = 0;

    /* Execute the command with combined arguments */
    if(fork() == 0){
      exec(args[0], args);
      fprintf(2, "xargs: exec %s failed\n", args[0]);
      exit(1);
    }
    wait(0);
    if(r != 1)
      break;
  }
  exit(0);
}
```

### 5.3 Execution Trace and Verification
```bash
$ echo hello too | xargs echo bye
bye hello too
```
The arguments `hello` and `too` were parsed from standard input and appended to `echo bye`, correctly producing `bye hello too`.

---

## 6. Conclusions

1. **Process Lifecycle Parallels:** Transitioning `fork()`, `exec()`, and `wait()` from Linux to xv6 illustrated that the Unix process execution model is completely independent of the underlying C library (glibc).
2. **Unidirectional Pipe Discipline:** Implementing `pingpong` reinforced why bidirectional IPC requires two independent pipes to avoid race conditions and self-reading. Synchronization between parent and child is mediated by the blocking semantics of `read()`.
3. **Concurrent Streaming Architectures:** Doug McIlroy's prime sieve illustrates how complex computational tasks can be decomposed into pipelines of independent filter stages, with data flowing continuously through kernel pipe buffers.
4. **Critical Kernel Resource Boundaries:** The limit 280 experiment highlighted the hierarchy of operating system resource constraints (`NOFILE = 16`, `NPROC = 64`, and `NFILE = 100`).
5. **Command Composability via `xargs`:** Writing `xargs` showed how user-space utilities can bridge pipeline streams to argument-based command invocation.
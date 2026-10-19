# Laboratory Work No. 8: Processes Talking Through Pipes, in xv6

**Course:** Operating Systems (Track B: build your own small OS (xv6))  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Laboratory Objectives
Inside the xv6 operating system, there is no glibc, no `strace`, and no manual pages: only the 22 fundamental system calls provided directly by the compiled kernel. The objective of Laboratory Work Nr. 8 is to utilize low-level process management and inter-process communication (IPC) system calls directly within xv6:
- **Process and Pipe Primitives:** Mastering the low-level system calls declared in `user/user.h`: `fork()`, `exit()`, `wait()`, `pipe()`, `read()`, `write()`, `close()`, `getpid()`, and `printf()`.
- **Bidirectional Communication (`pingpong`):** Coordinating two processes via two distinct anonymous pipes to exchange a single byte back and forth without data races, while ensuring all unused pipe descriptors are cleanly closed.
- **Concurrent Prime Sieve (`primes`):** Implementing Doug McIlroy's classic concurrent prime sieve design, chaining independent filter processes connected via anonymous pipes, dynamically discovering and outputting prime numbers up to a specified limit.
- **Avoiding Real Systems Traps:**
  1. *File descriptor exhaustion:* Respecting process-level limits (`NOFILE = 16` in `kernel/param.h`) and system-wide file table limits (`NFILE = 100`) by closing every inherited and unneeded pipe end.
  2. *Unbounded stream blocking:* Ensuring that all write descriptors are closed once generation ends so downstream filters observe `read() == 0` (EOF).
  3. *Compiler recursion constraints:* Handling xv6's strict compiler flags (`-Werror=infinite-recursion`) by annotating recursive functions with `__attribute__((noreturn))`.
- **Command Argument Orchestration (`xargs` - Stretch Goal):** Implementing the UNIX `xargs` utility for xv6, parsing standard input line-by-line and executing target binaries with combined argument lists.

### 1.2 Core Architectural Primitives in xv6
1. **Pipes and Ring Buffers:** In xv6 (`kernel/pipe.c`), an anonymous pipe is represented by `struct pipe`, comprising a 512-byte circular buffer (`char data[PIPESIZE]`), read/write byte offsets (`nread`, `nwrite`), and boolean flags indicating whether the read or write ends are open (`readopen`, `writeopen`). Access is synchronized with a spinlock.
2. **Blocking and Waking:** When a process reads from an empty pipe, the kernel calls `sleep(&pi->nread, &pi->lock)` to yield the CPU. When another process issues a `write()`, it invokes `wakeup(&pi->nread)`, moving suspended readers back to the `RUNNABLE` state.
3. **End-of-File (EOF) Semantics:** A reader receives an EOF (`read()` returns `0`) if and only if the pipe buffer is completely empty **and** `pi->writeopen == 0`. If even a single process in the entire operating system retains an open descriptor to the write end, readers will block indefinitely waiting for future data.

---

## 2. Part 1: Ping-Pong Communication (`pingpong.c`)

### 2.1 Implementation Details
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

### 2.2 Compilation and Execution Trace
The utility was registered in the xv6 `Makefile` under `UPROGS`:
```makefile
UPROGS=\
    $U/_cat\
    ...
    $U/_zombie\
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

### 2.3 Answers to Questions (Observe — Part 1)

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

## 3. Part 2: Concurrent Prime Sieve (`primes.c`)

### 3.1 Design of the Concurrent Sieve
Doug McIlroy (inventor of the UNIX pipe) conceived the concurrent prime sieve:
- A generating process feeds a stream of consecutive integers ($2, 3, \dots, 35$) into an initial pipe.
- Each filter stage reads its first incoming number $p$. By definition of the sieve, $p$ is prime, so the stage immediately prints `prime p`.
- The stage then creates an outgoing pipe, forks a child process to serve as the next stage, and forwards through the pipe only those subsequent numbers $n$ that are **not** divisible by $p$ ($n \pmod p \neq 0$).
- When the incoming pipe reaches EOF (`read() == 0`), the stage closes its outgoing pipe, waits for its child to exit, and terminates.

### 3.2 Implementation of `user/primes.c`
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

### 3.3 Sieve Pipeline Architecture
The streaming architecture can be represented as a chain of processes connected by pipes:

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

### 3.4 Execution Trace
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

### 3.5 Answers to Questions (Observe — Part 2)

#### (1) How many processes exist at the moment "prime 31" is printed?
At the moment `"prime 31"` is printed, there are **12 processes** simultaneously alive:
1. The **main generator process**, which fed the numbers $2 \dots 35$ into the initial pipe and is currently waiting in `wait(0)` for the first child to complete.
2. Plus **11 stage processes**, each corresponding to one prime discovered so far:
   - Stage 1: $p = 2$
   - Stage 2: $p = 3$
   - Stage 3: $p = 5$
   - Stage 4: $p = 7$
   - Stage 5: $p = 11$
   - Stage 6: $p = 13$
   - Stage 7: $p = 17$
   - Stage 8: $p = 19$
   - Stage 9: $p = 23$
   - Stage 10: $p = 29$
   - Stage 11: $p = 31$

Because all stages remain alive to pass data and only exit after their input stream terminates, all 11 stage processes and the main generator exist concurrently:
$$1 \text{ (main)} + 11 \text{ (stages)} = \mathbf{12\text{ processes}}.$$

#### (2) Change the limit from 35 to 280. Does it still work? If not, which limit did you hit (NOFILE per process, NPROC processes, or NFILE open files in the whole system) and how did you find out?
- **Does it work?** No, changing the limit to 280 causes execution to fail. It correctly prints 49 primes up to **227**, and then fails on the next allocation with the diagnostic message:
  ```
  primes: pipe failed after prime 227
  ```
- **Which limit was hit?** We hit **`NFILE`**, the **system-wide file table limit** (`NFILE = 100` in `kernel/param.h`).
- **Why `NFILE` instead of `NOFILE` or `NPROC`?**
  1. *Per-process descriptor limit (`NOFILE = 16`):* Because each stage rigorously closes unused descriptors (closing `fd[1]` in the child, `fd[0]` in the parent, and inherited descriptors), each stage holds at most 4 open descriptors (stdin, stdout, stderr, and its pipe descriptors). This is well below `NOFILE = 16`.
  2. *Process table limit (`NPROC = 64`):* Printing 49 primes requires 49 stages plus 1 main process = 50 processes. This is below `NPROC = 64`.
  3. *System-wide file table (`NFILE = 100`):* In xv6, every pipe requires **two** `struct file` entries in the kernel's global file table `ftable` (`kernel/file.c`)—one for the read end and one for the write end. The shell and standard I/O hold several file slots. With 49 active pipes connecting the stages, $49 \times 2 = 98$ file table slots are consumed by the pipes alone. When prime 227 attempts to allocate the 50th pipe via `sys_pipe()`, `filealloc()` fails because all 100 slots in `ftable` are occupied, returning $-1$.
- **How we found out:**
  We identified this by observing the return code check on `pipe(fd) < 0` inside `primes.c`, which printed `primes: pipe failed after prime 227`. Examining `kernel/sysfile.c` confirms that `sys_pipe()` calls `filealloc()` twice; when all 100 entries in `ftable.file` are active, `filealloc()` returns null, causing `pipe()` to fail.

#### (3) In two sentences: why is this a sieve that runs in parallel?
Each prime filter stage executes as an independent concurrent process that reads from its input pipe and streams non-multiples into its output pipe as soon as numbers arrive, rather than waiting for preceding stages to finish filtering the entire dataset. Consequently, multiple candidate numbers at different stages of the sieve are processed simultaneously across pipeline stages, achieving true streaming pipeline parallelism across CPU cores.

---

## 4. Part 3: Stretch Assignment — Implementing `xargs` (`xargs.c`)

### 4.1 Specification and Design
The `xargs` utility reads arguments from standard input separated by newlines and executes a specified command once per line, appending the arguments from the input line to the end of the provided command arguments:
- In xv6 there is no `$PATH` environment variable, so `exec()` is called with the exact binary name provided.
- Arguments on an input line can be separated by spaces, which must be split into distinct argument strings.
- For each input line, `xargs` forks a child process, executes the command with `exec()`, and waits for the child to exit before reading the next line.

### 4.2 Implementation of `user/xargs.c`

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

### 4.3 Execution Trace and Verification
We tested `xargs` inside xv6 with command-line piping:
```bash
$ echo hello too | xargs echo bye
bye hello too
```
The arguments `hello` and `too` were parsed from standard input and appended to `echo bye`, correctly producing `bye hello too`.

---

## 5. Conclusions

Laboratory Work No. 8 demonstrated the elegance, efficiency, and real-world resource constraints of inter-process communication in a minimalist UNIX kernel:

1. **Unidirectional Pipe Discipline:** Implementing `pingpong` reinforced why bidirectional IPC requires two independent pipes to avoid race conditions and self-reading. Synchronization between parent and child is mediated by the blocking semantics of `read()`.
2. **Concurrent Streaming Architectures:** Doug McIlroy's prime sieve illustrates how complex computational tasks can be decomposed into pipelines of independent filter stages. Concurrency is achieved through stream processing, with data flowing continuously through kernel pipe buffers.
3. **Critical Kernel Resource Boundaries:** The limit 280 experiment highlighted the hierarchy of operating system resource constraints. While per-process descriptor limits (`NOFILE = 16`) and process limits (`NPROC = 64`) were respected, the architecture reached the system-wide open file table limit (`NFILE = 100`), demonstrating how kernel data structure sizing defines system scalability.
4. **Command Composability via `xargs`:** Writing `xargs` showed how user-space utilities can bridge pipeline streams to argument-based command invocation, relying on standard UNIX system primitives (`fork`, `exec`, `wait`).
# Laboratory Work No. 7: Processes, Signals, and Pipelines

**Course:** Operating Systems (Track A: Linux)  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Laboratory Objectives
The primary objective of Laboratory Work Nr. 7 is to explore the Linux process model, process execution lifecycle, standard stream redirection, and inter-process communication (IPC) via pipes. Core competencies investigated include:
- **Tracing Process Syscalls (`strace`):** Observing how the shell orchestrates child processes, pipeline creation (`pipe2`), process cloning (`clone`), descriptor remapping (`dup2`), and program execution (`execve`).
- **Process Creation and Lifecycle (`fork` and `waitpid`):** Understanding address space duplication, parent-child concurrency, exit codes, and status retrieval via POSIX macros (`WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED`, `WTERMSIG`).
- **Zombie Process Mechanics:** Understanding how and why defunct processes occur when children terminate without their exit status being reaped by the parent.
- **Process Tree Exponential Growth:** Analyzing the mathematical and architectural behavior of repeated `fork()` invocations.
- **I/O Buffering Semantics:** Identifying the fundamental distinction between line-buffered interactive terminals (TTY) and block-buffered pipes/files, and its impact on inherited libc buffers across `fork()`.
- **Program Execution and Signal Trapping (`myrun`):** Implementing a robust process runner using `execvp` that distinguishes between normal process exit codes and asynchronous signal terminations (`SIGTERM`).
- **I/O Redirection (`dup2` and `myrun2`):** Redirecting standard streams (`stdin`, `stdout`) to regular files by manipulating per-process file descriptor tables.
- **Multi-Stage Pipelines (`pipeline` and `pipeline3`):** Constructing two-stage and three-stage inter-process communication pipelines using anonymous pipes (`pipe`), coordinating data flow, and preventing deadlocks by properly closing unneeded pipe descriptors to propagate EOF.

### 1.2 Theoretical Concepts
1. **The Fork-Exec-Wait Paradigm:** UNIX separates process creation from binary execution. `fork()` creates an exact clone of the caller's virtual address space (optimized via Copy-on-Write). `execve()` replaces the virtual address space with a new executable while preserving the process identifier (PID) and open file descriptor table (unless `O_CLOEXEC` is set). The parent process synchronizes with child termination and harvests the exit status using `wait()` or `waitpid()`.
2. **Zombie and Orphan States:** When a child process terminates via `exit()`, its memory and open files are released, but its entry in the kernel's process table (task struct) is preserved in state `Z` (Zombie / Defunct) until the parent acknowledges its exit via `waitpid()`. If the parent exits before the child, the child becomes an orphan and is automatically reparented to the system init process (`systemd` / PID 1), which reaps it.
3. **File Descriptors and the Open File Table:** Every process possesses an open file descriptor table. Descriptors 0, 1, and 2 represent `stdin`, `stdout`, and `stderr`. System call `dup2(oldfd, newfd)` atomically closes `newfd` (if open) and duplicates `oldfd` into the slot `newfd`, redirecting stream I/O.
4. **Anonymous Pipes:** A unidirectional data channel maintained in kernel memory with a read end (`p[0]`) and a write end (`p[1]`). A reader encountering an empty pipe blocks until data is written, or returns `0` (EOF) if and only if **all** write descriptors to the pipe across the entire system have been closed. Failing to close write ends in parents or children leads to permanent read deadlocks.

---

## 2. Part 1: Tracing Execution Mechanics with `strace`

### 2.1 Tracing a Pipelined Command
We executed `strace` to trace all process management, stream duplication, and pipeline syscalls executed by `bash` when evaluating `ls / | wc -l`:

```bash
~/os-lab7 ❯ strace -f -e trace=pipe2,clone,clone3,dup2,execve,wait4 bash -c 'ls / | wc -l'
execve("/usr/bin/bash", ["bash", "-c", "ls / | wc -l"], 0x7ffd1ada4d18 /* 199 vars */) = 0
pipe2([3, 4], 0)                        = 0
clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7eb6a3a42e50) = 15021
strace: Process 15021 attached
[pid 15020] clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7eb6a3a42e50) = 15022
[pid 15021] dup2(4, 1)                  = 1
strace: Process 15022 attached
[pid 15020] wait4(-1 <unfinished ...>
[pid 15022] dup2(3, 0)                  = 0
[pid 15021] execve("/usr/bin/ls", ["ls", "/"], 0x5bf3950f21b0 /* 199 vars */ <unfinished ...>
[pid 15022] execve("/usr/bin/wc", ["wc", "-l"], 0x5bf3950f21b0 /* 199 vars */ <unfinished ...>
[pid 15021] <... execve resumed>)       = 0
[pid 15021] +++ exited with 0 +++
[pid 15020] <... wait4 resumed>, [{WIFEXITED(s) && WEXITSTATUS(s) == 0}], 0, NULL) = 15021
[pid 15020] wait4(-1 <unfinished ...>
[pid 15022] <... execve resumed>)       = 0
19
[pid 15022] +++ exited with 0 +++
<... wait4 resumed>, [{WIFEXITED(s) && WEXITSTATUS(s) == 0}], 0, NULL) = 15022
--- SIGCHLD {si_signo=SIGCHLD, si_code=CLD_EXITED, si_pid=15021, si_uid=1000, si_status=0, si_utime=0, si_stime=0} ---
wait4(-1, 0x7ffeff115694, WNOHANG, NULL) = -1 ECHILD (No child processes)
+++ exited with 0 +++
```

### 2.2 System Call Profiling of `ls`
We profiled the system calls executed by `ls` when reading `/usr/bin`:

```bash
~/os-lab7 ❯ strace -c ls /usr/bin > /dev/null
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 56.20    0.000843         140         6           getdents64
 25.73    0.000386         386         1           execve
  7.67    0.000115           8        14           mmap
  2.60    0.000039           9         4           mremap
  1.53    0.000023           4         5           openat
  1.47    0.000022           3         6           brk
  1.27    0.000019           6         3           newfstatat
  1.13    0.000017           2         6           fstat
  0.73    0.000011           1         7           close
  0.67    0.000010           0        12           write
  0.60    0.000009           4         2           read
  0.33    0.000005           5         1         1 access
  0.07    0.000001           0         2         2 ioctl
------ ----------- ----------- --------- --------- ----------------
100.00    0.001500          17        87         7 total
```

### 2.3 Answers to Questions (Part 1)

#### (1) In what order do `pipe2`, `clone`, `dup2` and `execve` appear? Which process (PID) calls `dup2(.., 1)` and which calls `dup2(.., 0)`? Explain why in one sentence each.
- **Execution Order:**
  1. `pipe2([3, 4], 0)` (Parent PID 15020 creates the pipe before cloning children)
  2. `clone(...) = 15021` (Parent clones first child PID 15021)
  3. `clone(...) = 15022` (Parent clones second child PID 15022)
  4. `dup2(...)` (Each child redirects its respective descriptor)
  5. `execve(...)` (Each child loads its program binary)
- **`dup2(4, 1)` caller:** PID 15021 (`ls`) calls `dup2(4, 1)` because it is the pipeline producer and must redirect standard output (`stdout`, fd 1) to the pipe's write end (`fd 4`).
- **`dup2(3, 0)` caller:** PID 15022 (`wc`) calls `dup2(3, 0)` because it is the pipeline consumer and must redirect standard input (`stdin`, fd 0) to the pipe's read end (`fd 3`).

#### (2) On Linux there is no "fork" line. Which call creates the children instead?
On modern Linux kernels, the **`clone`** (or `clone3`) system call creates the children instead of `fork`. In glibc, standard `fork()` is implemented as a wrapper around `clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, ...)`. The `clone` syscall generalizes process and thread creation by allowing fine-grained configuration over what memory, file tables, and namespaces are shared or copied.

#### (3) How many write calls does `ls` make to print all of `/usr/bin`? Why so few?
`ls` makes only **12 `write` calls** to output thousands of file entries from `/usr/bin`. This occurs because standard I/O streams (`libc` / `stdout`) use large user-space buffers (typically 4096 or 8192 bytes). Instead of executing an expensive kernel context switch for every single filename, `ls` formats and aggregates filenames inside user memory and emits them in large contiguous block writes.

---

## 3. Part 2: Process Creation, Zombies, Quiz, and I/O Buffering

### 3.1 Basic Fork and Status Inspection (`fork.c`)
We created `fork.c` to explore `fork()` and `waitpid()`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    printf("before fork: pid=%d\n", getpid());
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) { 
        perror("fork"); 
        exit(1); 
    }

    if (pid == 0) { /* child */
        printf("child: pid=%d parent=%d\n", getpid(), getppid());
        exit(42);
    }

    int status; /* parent */
    waitpid(pid, &status, 0);
    printf("parent: pid=%d, my child %d exited with %d\n",
           getpid(), pid, WEXITSTATUS(status));
    return 0;
}
```

Compilation and execution trace:
```bash
~/os-lab7 ❯ gcc -Wall -Wextra -o fork fork.c && ./fork
before fork: pid=16968
child: pid=16969 parent=16968
parent: pid=16968, my child 16969 exited with 42
```

### 3.2 Observing Zombie Processes (`zombie.c`)
We wrote `zombie.c` to observe an un-reaped child process:

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        printf("Child (PID %d) is exiting now...\n", getpid());
        exit(0);
    } else {
        printf("Parent (PID %d) created child (PID %d).\n", getpid(), pid);
        printf("Parent sleeping for 30 seconds. Check `ps` in another terminal!\n");
        sleep(30);
        printf("Parent waking up and exiting.\n");
    }

    return 0;
}
```

Execution trace and process table inspection:
```bash
~/os-lab7 ❯ gcc -Wall -Wextra -o zombie zombie.c
~/os-lab7 ❯ ./zombie &
[2] 17428
Parent (PID 17428) created child (PID 17430).
Parent sleeping for 30 seconds. Check `ps` in another terminal!
Child (PID 17430) is exiting now...

~/os-lab7 ❯ sleep 1; ps -o pid,ppid,stat,cmd --ppid $!
    PID    PPID STAT CMD
  17430   17428 Z    [zombie] <defunct>

~/os-lab7 ❯ Parent waking up and exiting.
```

### 3.3 The Process Tree Quiz (`quiz.c`)
We tested the behavior of three successive `fork()` calls:

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    fork();
    fork();
    fork();
    printf("hi\n");
    return 0;
}
```

Execution and line count:
```bash
~/os-lab7 ❯ gcc -Wall -o quiz quiz.c && ./quiz | wc -l
8
```

### 3.4 I/O Buffering Mechanics (`./fork` vs. `./fork | cat`)
We evaluated `fork.c` with `fflush(stdout)` removed:

```bash
~/os-lab7 ❯ ./fork
before fork: pid=18134
child: pid=18135 parent=18134
parent: pid=18134, my child 18135 exited with 42

~/os-lab7 ❯ ./fork | cat
before fork: pid=18198
child: pid=18200 parent=18198
before fork: pid=18198
parent: pid=18198, my child 18200 exited with 42
```

### 3.5 Answers to Questions (Part 2)

#### (1) What does the STAT column show for the child while the parent sleeps, and what happens to it after the parent calls `waitpid`?
- **While parent sleeps:** The STAT column displays **`Z`** (`[zombie] <defunct>`). The child process has finished execution and released its memory space, but its task descriptor remains in the kernel's process table so its termination status can be delivered to the parent.
- **After `waitpid` is called (or parent exits):** The parent reads the exit status, and the kernel completely reaps the child's entry from the process table, freeing its PID. If the parent exits without calling `waitpid`, the zombie child is adopted by `init` (`PID 1`), which promptly executes `wait()` and cleans up the zombie.

#### (2) Your prediction for `quiz.c` and the real count. Draw the process tree (8 boxes).
- **Prediction:** 8 lines (or 2 if mistakenly assuming serial execution).
- **Real Count:** Exactly **8**.
- **Process Tree Analysis:** Each call to `fork()` duplicates all active processes, yielding $2^n$ processes after $n$ calls ($2^3 = 8$):

```
                       [ P0 (Root) ]
                      /             \
             (fork 1)                 (fork 1)
            /                                 \
      [ P0 ]                                   [ P1 ]
     /      \                                 /      \
 (fork 2)   (fork 2)                      (fork 2)   (fork 2)
   /          \                             /          \
 [ P0 ]        [ P2 ]                     [ P1 ]        [ P3 ]
 /    \        /    \                     /    \        /    \
(f3)  (f3)    (f3)  (f3)                 (f3)  (f3)    (f3)  (f3)
 |     |       |     |                    |     |       |     |
[P0]  [P4]    [P2]  [P5]                 [P1]  [P6]    [P3]  [P7]
  |     |       |     |                    |     |       |     |
 "hi"  "hi"    "hi"  "hi"                 "hi"  "hi"    "hi"  "hi"
```

All 8 processes terminate after printing `"hi"`, generating 8 lines of output.

#### (3) Without `fflush`, why does "before fork" appear twice only when the output goes into a pipe?
- **Terminal output (Line-Buffered):** When writing to an interactive terminal, `stdout` is line-buffered by default. The newline character `\n` in `printf("before fork: pid=%d\n", getpid())` immediately flushes the user-space libc buffer and triggers a `write()` system call *before* `fork()` is called. The buffer is empty when the child process is created.
- **Pipe output (Block-Buffered):** When redirected into a pipe (`./fork | cat`), `stdout` switches to fully buffered block mode (typically 4096 bytes). The string `"before fork...\n"` is retained in libc's in-memory stream buffer. When `fork()` executes, it creates a Copy-On-Write duplicate of the parent's entire memory space, **including the unflushed buffer**. As a result, both the parent and the child possess identical copies of the buffer containing the unprinted message. Upon process exit, both processes flush their private buffers into the pipe, printing the message twice.

---

## 4. Part 3: Program Execution and Signal Handling (`myrun.c`)

### 4.1 Implementation of `myrun.c`
We built `myrun.c` to execute an arbitrary command with arguments, wait for its completion, and report whether it exited normally with an exit code or was terminated by a signal:

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

/* usage: ./myrun program [args...] e.g. ./myrun ls -l /tmp */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s program [args...]\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Replace process with target binary, searching PATH */
        execvp(argv[1], &argv[1]);

        /* If execvp returns, an error occurred */
        perror("execvp");
        exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    /* Inspect termination status */
    if (WIFEXITED(status)) {
        printf("[myrun] %s exited with status %d\n", argv[1], WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[myrun] %s killed by signal %d\n", argv[1], WTERMSIG(status));
    }

    return 0;
}
```

### 4.2 Execution Traces and Verification
We tested `myrun` against four distinct execution scenarios:

1. **Successful Execution (`status 0`):**
   ```bash
   ~/os-lab7 ❯ ./myrun ls -d /etc
   /etc
   [myrun] ls exited with status 0
   ```

2. **Command Reporting Failure (`status 1`):**
   ```bash
   ~/os-lab7 ❯ ./myrun false
   [myrun] false exited with status 1
   ```

3. **Non-Existent Command (`status 127`):**
   ```bash
   ~/os-lab7 ❯ ./myrun nosuchprog
   execvp: No such file or directory
   [myrun] nosuchprog exited with status 127
   ```

4. **Process Killed by Asynchronous Signal (`signal 15 - SIGTERM`):**
   ```bash
   ~/os-lab7 ❯ ./myrun sleep 100 & sleep 1; pkill -x sleep
   [1] 20105
   [myrun] sleep killed by signal 15
   ```

---

## 5. Part 4: File Descriptor Redirection (`redirect.c` & `myrun2.c`)

### 5.1 Basic Output Redirection (`redirect.c`)
We implemented `redirect.c` to duplicate an opened file descriptor onto `STDOUT_FILENO` (`fd 1`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* Child: redirect stdout (fd 1) to out.txt */
        int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }

        dup2(fd, 1);    /* stdout now points to out.txt */
        close(fd);      /* close original fd; descriptor 1 remains open */

        execlp("ls", "ls", "-l", (char *)NULL);
        perror("execlp");
        exit(127);
    }

    /* Parent */
    int status;
    waitpid(pid, &status, 0);
    return 0;
}
```

Execution and output verification:
```bash
~/os-lab7 ✗ ./redirect && cat out.txt | head -3
total 120
-rwxr-xr-x 1 sunflower sunflower 16328 Oct 18 18:12 fork
-rw-r--r-- 1 sunflower sunflower  3834 Oct 18 18:12 fork.c
```

### 5.2 Generalized Command Runner with File Output (`myrun2.c`)
We extended `myrun` to support writing the executed process's stdout directly to an output file:

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

/* usage: ./myrun2 outfile program [args...] */
int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s outfile program [args...]\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);

        execvp(argv[2], &argv[2]);
        perror("execvp");
        exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        printf("[myrun2] %s exited with status %d (output written to %s)\n",
               argv[2], WEXITSTATUS(status), argv[1]);
    } else if (WIFSIGNALED(status)) {
        printf("[myrun2] %s killed by signal %d\n", argv[2], WTERMSIG(status));
    }

    return 0;
}
```

Verification:
```bash
~/os-lab7 ❯ ./myrun2 out.txt ls -l /etc
[myrun2] ls exited with status 0 (output written to out.txt)
```

---

## 6. Part 5: Inter-Process Communication with Pipelines

### 6.1 Two-Stage Pipeline (`pipeline.c`)
We implemented `pipeline.c` to connect `ls -l /etc` directly to `wc -l` via an anonymous pipe:

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    int p[2];
    if (pipe(p) < 0) {
        perror("pipe");
        exit(1);
    }

    /* Child 1: ls -l /etc */
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid1 == 0) {
        close(p[0]);               /* not reading */
        dup2(p[1], STDOUT_FILENO); /* redirect stdout to pipe write end */
        close(p[1]);

        execlp("ls", "ls", "-l", "/etc", (char *)NULL);
        perror("execlp ls");
        exit(127);
    }

    /* Child 2: wc -l */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid2 == 0) {
        close(p[1]);              /* not writing */
        dup2(p[0], STDIN_FILENO); /* redirect stdin to pipe read end */
        close(p[0]);

        execlp("wc", "wc", "-l", (char *)NULL);
        perror("execlp wc");
        exit(127);
    }

    /* Parent closes both pipe ends so EOF can be reached */
    close(p[0]);
    close(p[1]);

    /* Wait for both children */
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}
```

Execution trace:
```bash
~/os-lab7 ❯ ./pipeline
192
```

### 6.2 Three-Stage Pipeline (`pipeline3.c`)
We built a three-stage pipeline executing `ls /usr/bin | grep x | wc -l` using two anonymous pipes (`p1` and `p2`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    int p1[2], p2[2];
    if (pipe(p1) < 0 || pipe(p2) < 0) {
        perror("pipe");
        exit(1);
    }

    /* Stage 1: ls /usr/bin -> p1[1] */
    if (fork() == 0) {
        dup2(p1[1], STDOUT_FILENO);
        close(p1[0]); close(p1[1]);
        close(p2[0]); close(p2[1]);
        execlp("ls", "ls", "/usr/bin", (char *)NULL);
        exit(127);
    }

    /* Stage 2: p1[0] -> grep x -> p2[1] */
    if (fork() == 0) {
        dup2(p1[0], STDIN_FILENO);
        dup2(p2[1], STDOUT_FILENO);
        close(p1[0]); close(p1[1]);
        close(p2[0]); close(p2[1]);
        execlp("grep", "grep", "x", (char *)NULL);
        exit(127);
    }

    /* Stage 3: p2[0] -> wc -l */
    if (fork() == 0) {
        dup2(p2[0], STDIN_FILENO);
        close(p1[0]); close(p1[1]);
        close(p2[0]); close(p2[1]);
        execlp("wc", "wc", "-l", (char *)NULL);
        exit(127);
    }

    /* Parent closes all pipe ends */
    close(p1[0]); close(p1[1]);
    close(p2[0]); close(p2[1]);

    /* Wait for all 3 children */
    while (wait(NULL) > 0);

    return 0;
}
```

Verification against native shell pipeline:
```bash
~/os-lab7 ❯ ./pipeline3
552

~/os-lab7 ❯ \ls /usr/bin | grep x | wc -l
552
```
Both outputs produce **552**, verifying identical output and stream fidelity.

### 6.3 Crucial Descriptor Closing Rule (Avoiding Pipe Deadlocks)
A critical insight in pipeline implementation is descriptor hygiene:
- If the parent process or child stages omit closing unused pipe write descriptors (e.g. `p1[1]` or `p2[1]`), the kernel's reference counter for the write end remains greater than zero.
- Consequently, downstream consumers like `wc` will block perpetually inside the `read()` system call waiting for additional data, causing a permanent deadlock.
- Explicitly closing both ends in the parent process and closing all non-participating pipe ends in each child ensures that once the upstream stage exits, the write end count drops to zero, delivering `EOF` (`0` bytes read) to the downstream stage.

---

## 7. Conclusions

Laboratory Work No. 7 provided fundamental systems-level experience with UNIX process creation, process coordination, file descriptor routing, and inter-process communication:
1. **Process Orchestration with `clone`/`fork` and `execve`**: System tracing demonstrated that command execution decouples address space creation from binary loading. The shell leverages this separation to configure standard file descriptors before the executable starts.
2. **Lifecycle and Zombie Management**: Processes do not vanish upon calling `exit()`; their metadata remains in state `Z` until their parent invokes `waitpid()`. Understanding zombie management is essential for avoiding process table exhaustion in long-running services.
3. **Buffering Subtleties Across Address Spaces**: The `fork.c` experiment clearly proved that libc user-space stream buffering changes between interactive TTYs (line-buffered) and pipes (block-buffered). Unflushed buffers are duplicated during `fork()`, resulting in duplicated outputs unless explicitly flushed via `fflush()`.
4. **Signal and Exit Discrimination**: Implementing `myrun.c` demystified the POSIX status integer, distinguishing normal program termination (`WIFEXITED`, `WEXITSTATUS`) from asynchronous signal termination (`WIFSIGNALED`, `WTERMSIG`).
5. **Robust Pipeline Construction**: Multi-stage pipelines rely on strict descriptor discipline. By manipulating descriptor tables with `dup2` and immediately closing superfluous pipe ends, multi-stage pipelines stream data smoothly and terminate deterministically upon reaching EOF.
# Laboratory Work No. 9: Watching System Calls with `strace`

**Course:** Operating Systems (Track A: the real OS, from the outside (Linux))  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Laboratory Objectives
The boundary between a user-space program and the operating system kernel is crossed through **system calls**. Whereas ordinary C functions execute entirely inside the process's user-space memory and stack, system calls transition the CPU into kernel mode so the OS can safely perform privileged I/O, filesystem, and process operations. The objectives of Laboratory Work No. 9 are:
- **Observing Live System Calls (`strace`):** Using `strace` to intercept and display every system call issued by an unmodified Linux utility (`ls -l`), including arguments and return values.
- **Direct Kernel I/O in C (`syscalls.c`):** Writing, compiling, and running a minimal C program that interacts with the filesystem directly through POSIX system calls (`open`, `write`, `read`, `close`) rather than buffered `<stdio.h>` file streams (`fopen`, `fprintf`, `fread`).
- **Understanding File Descriptors:** Observing how the kernel assigns file descriptors starting at `3` because descriptors `0`, `1`, and `2` are already reserved at process startup.
- **Quantifying System Call Usage:** Filtering and counting specific system calls (`write`, `openat`) across programs, and observing how C library (`glibc`) stream buffering reduces kernel boundary crossings.

### 1.2 System Call Mechanics and File Descriptors
1. **How `strace` Works:** `strace` attaches to a target process via the Linux `ptrace()` system call, suspending the traced process on every system call entry and exit to inspect CPU registers, decode syscall numbers and arguments, and print the return value (or negative `errno` code such as `ENOENT`).
2. **File Descriptor Table:** Every process maintains a per-process table of open file descriptors indexed by non-negative integers. By POSIX convention, every newly launched process inherits three pre-opened descriptors:
   - `0` (`STDIN_FILENO`): Standard input (`stdin`)
   - `1` (`STDOUT_FILENO`): Standard output (`stdout`)
   - `2` (`STDERR_FILENO`): Standard error (`stderr`)
   When a program calls `open()` or `openat()`, the kernel allocates the **lowest-numbered unused descriptor** (typically `3`).
3. **`open()` vs. `openat()` on Modern Linux:** In C source code, calling `open("note.txt", ...)` invokes the `glibc` wrapper function, which on modern Linux architectures issues the `openat(AT_FDCWD, "note.txt", ...)` kernel system call (`AT_FDCWD` specifies resolution relative to the current working directory).
4. **Raw Syscalls vs. `libc` Buffering:** Calling `write()` directly triggers a kernel mode switch immediately for every call. By contrast, `printf()` writes into a user-space `libc` buffer attached to `stdout`:
   - Connected to an interactive terminal (**TTY**), `stdout` is **line-buffered** (flushes on every `\n`).
   - Redirected to a file, pipe, or `/dev/null`, `stdout` becomes **block-buffered** (typically 4 KiB), accumulating multiple `printf()` calls into a single `write(1, ...)` syscall upon flush or exit.

---

## 2. Part 1: Watching System Calls of a Real Command (`ls -l`)

### 2.1 Tracing File and I/O System Calls of `ls -l`
We ran `strace` filtering for `openat`, `read`, `write`, and `close` on `ls -l`:

```bash
$ strace -f -e trace=openat,read,write,close ls -l
```

An annotated excerpt from the captured trace (`results.txt` and live rerun) shows the sequence of phases inside a single `ls -l` invocation:

```text
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
close(3)                                = 0
openat(AT_FDCWD, "/usr/lib/libcap.so.2", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\320{\0\0\0\0\0\0"..., 1024) = 1024
close(3)                                = 0
openat(AT_FDCWD, "/usr/lib/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\300y\2\0\0\0\0\0"..., 1024) = 1024
close(3)                                = 0
...
openat(AT_FDCWD, ".", O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_DIRECTORY) = 3
openat(AT_FDCWD, "/etc/nsswitch.conf", O_RDONLY|O_CLOEXEC) = 4
read(4, "# Name Service Switch configurat"..., 4096) = 372
read(4, "", 4096)                       = 0
close(4)                                = 0
openat(AT_FDCWD, "/etc/passwd", O_RDONLY|O_CLOEXEC) = 4
read(4, "root:x:0:0::/root:/usr/bin/bash\n"..., 4096) = 1615
close(4)                                = 0
openat(AT_FDCWD, "/etc/group", O_RDONLY|O_CLOEXEC) = 4
read(4, "root:x:0:root\nsys:x:3:bin\nmem:x:"..., 4096) = 930
close(4)                                = 0
...
close(3)                                = 0
openat(AT_FDCWD, "/etc/localtime", O_RDONLY|O_CLOEXEC) = 3
read(3, "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\17\0\0\0\17\0\0\0\0"..., 4096) = 2120
read(3, "TZif2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\17\0\0\0\17\0\0\0\0"..., 4096) = 1317
close(3)                                = 0
write(1, "total 96\n-rw-r--r-- 1 sunflower "..., 444) = 444
close(1)                                = 0
close(2)                                = 0
+++ exited with 0 +++
```

During execution, `ls -l`:
1. Loads shared libraries (`ld.so.cache`, `libcap.so.2`, `libc.so.6`) via `openat`/`read`/`close`.
2. Attempts locale translation lookups (`coreutils.mo`, returning `-1 ENOENT` when absent).
3. Opens the current directory `.` (`O_DIRECTORY`), resolves UID/GID names via `/etc/nsswitch.conf`, `/etc/passwd`, `/etc/group`, and `libnss_systemd`, and loads `/etc/localtime` to format file modification timestamps.
4. Prints the formatted directory listing using `write(1, ...)` and closes standard descriptors before exiting with status `0`.

### 2.2 Counting Total System Calls (`strace -c ls -l`)
We profiled all system calls made by `ls -l` using `strace -c ls -l >/dev/null`:

```text
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 31.16    0.000592           8        68        29 openat
 20.32    0.000386         386         1           execve
 12.68    0.000241          11        21           mmap
  8.53    0.000162           3        41           close
  7.11    0.000135           4        33           statx
  4.11    0.000078          13         6           mprotect
  2.47    0.000047           2        16           fstat
  2.32    0.000044           6         7           llistxattr
  2.05    0.000039           3        13           read
  1.79    0.000034           3         9           newfstatat
  1.42    0.000027           4         6         4 prctl
  0.95    0.000018           2         8           lseek
  0.79    0.000015           5         3           brk
  0.79    0.000015           3         4           rt_sigprocmask
  0.47    0.000009           4         2         2 ioctl
  0.47    0.000009           4         2           getdents64
  0.47    0.000009           4         2           getrandom
  0.42    0.000008           8         1         1 access
  0.37    0.000007           3         2           futex
  0.26    0.000005           5         1           write
  0.26    0.000005           5         1           prlimit64
  0.21    0.000004           4         1           arch_prctl
  0.21    0.000004           4         1           set_tid_address
  0.21    0.000004           4         1           set_robust_list
  0.16    0.000003           3         1           rseq
------ ----------- ----------- --------- --------- ----------------
100.00    0.001900           7       251        36 total
```

*Note on environment variance:* In the initial desktop terminal session (with `systemd-userdbd` socket accessible), `strace -c ls -l >/dev/null` recorded **260** total syscalls (**30** `openat`, **28** `close`, **15** `read`, **1** `write`, plus socket/epoll calls). Inside the isolated verification environment where `/run/systemd/userdb/` is unreachable, `libnss_systemd` falls back to probing `userdb` directories across `/etc`, `/run`, and `/usr`, resulting in **251** total syscalls (**68** `openat`, **41** `close`, **13** `read`, **1** `write`) and **123** filtered I/O calls (`openat`/`read`/`write`/`close`). In both environments, a single simple `ls -l` executes roughly **250–260** total system calls.

### 2.3 Answers to Questions (Observe — Part 1)
> **Which system call does `ls` use to print to the screen? To which descriptor does it write?**

- **System call:** `write` (e.g., `write(1, "total ...\n", ...)`).
- **File descriptor:** **`1`** (`STDOUT_FILENO`, standard output). When writing directly to an interactive terminal, `ls` emits one `write(1, ...)` call per output line (e.g., 1 header line + 1 line per entry); when redirected or piped, `libc` block-buffers `stdout` and emits all directory output in a single `write(1, ...)` call.

---

## 3. Part 2: A Program That Calls the Kernel Directly (`syscalls.c`)

### 3.1 Implementation (`syscalls.c`)
We implemented `syscalls.c` to create/truncate `note.txt`, write `"hello, kernel\n"` directly via `write()`, close the descriptor, reopen the file read-only, read the bytes back with `read()`, and print the counts to `stdout`:

```c
/* syscalls.c - open, write, read and close, using only system calls.
 * Build:  gcc -Wall -Wextra -o syscalls syscalls.c
 * Run:    ./syscalls
 * See the calls: strace ./syscalls
 */
#include <fcntl.h>   /* open */
#include <unistd.h>  /* write, read, close */
#include <string.h>  /* strlen */
#include <stdio.h>   /* printf */

int main(void) {
    const char *msg = "hello, kernel\n";

    /* open() is a system call: ask the kernel for a file, get back a descriptor */
    int fd = open("note.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return 1; }
    printf("open returned descriptor %d\n", fd);

    /* write() is a system call: hand the bytes to the kernel */
    ssize_t n = write(fd, msg, strlen(msg));
    if (n < 0) { perror("write"); close(fd); return 1; }
    printf("write sent %zd bytes\n", n);
    close(fd);

    /* read it back, through the kernel again */
    fd = open("note.txt", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    char buf[64];
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) { perror("read"); close(fd); return 1; }
    buf[n] = '\0';
    printf("read got %zd bytes: %s", n, buf);
    close(fd);

    return 0;
}
```

### 3.2 Compilation and Execution Verification
Building with `-Wall -Wextra` produces zero compiler warnings, and running `./syscalls` matches the expected output verbatim:

```bash
$ gcc -Wall -Wextra -o syscalls syscalls.c
$ ./syscalls
open returned descriptor 3
write sent 14 bytes
read got 14 bytes: hello, kernel
$ wc -c note.txt
14 note.txt
```

### 3.3 Watching System Calls of `./syscalls` with `strace`
Tracing `openat`, `write`, `read`, and `close` on `./syscalls` exposes both the dynamic linker initialization and our six explicit file I/O system calls:

```bash
$ strace -e trace=openat,write,read,close ./syscalls
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
close(3)                                = 0
openat(AT_FDCWD, "/usr/lib/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\300y\2\0\0\0\0\0"..., 1024) = 1024
close(3)                                = 0
openat(AT_FDCWD, "note.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3
write(1, "open returned descriptor 3\n", 27) = 27
write(3, "hello, kernel\n", 14)         = 14
write(1, "write sent 14 bytes\n", 20)   = 20
close(3)                                = 0
openat(AT_FDCWD, "note.txt", O_RDONLY)  = 3
read(3, "hello, kernel\n", 63)          = 14
write(1, "read got 14 bytes: hello, kernel"..., 33) = 33
close(3)                                = 0
+++ exited with 0 +++
```

Mapping each C statement in `syscalls.c` to its kernel call:

| C Statement in `syscalls.c` | Observed Kernel System Call | Return Value |
| :--- | :--- | :--- |
| `open("note.txt", O_CREAT \| O_WRONLY \| O_TRUNC, 0644)` | `openat(AT_FDCWD, "note.txt", O_WRONLY\|O_CREAT\|O_TRUNC, 0644)` | `3` (new fd) |
| `write(fd, msg, strlen(msg))` | `write(3, "hello, kernel\n", 14)` | `14` bytes written |
| `close(fd)` | `close(3)` | `0` (success) |
| `open("note.txt", O_RDONLY)` | `openat(AT_FDCWD, "note.txt", O_RDONLY)` | `3` (reused fd) |
| `read(fd, buf, sizeof(buf) - 1)` | `read(3, "hello, kernel\n", 63)` | `14` bytes read |
| `close(fd)` | `close(3)` | `0` (success) |

Running `strace -c ./syscalls >/dev/null` shows that the entire `./syscalls` program makes only **41** system calls total (18 distinct syscall types, including `execve`, `mmap`, `mprotect`, `brk`, `openat`, `close`, `write`, `read`, etc.).

### 3.4 Answers to Questions (Observe — Part 2)
> **The descriptor returned by the first `openat` is 3, not 0, 1 or 2. Why are those three already taken?**

Descriptors `0`, `1`, and `2` are already occupied before `main()` begins because every UNIX/Linux process starts with the three standard I/O streams already open (inherited from the parent shell):
- **`0` (`STDIN_FILENO`)**: Standard input (`stdin`)
- **`1` (`STDOUT_FILENO`)**: Standard output (`stdout`)
- **`2` (`STDERR_FILENO`)**: Standard error (`stderr`)

Because POSIX guarantees that `open()` / `openat()` always returns the **lowest-numbered unused file descriptor**, the first `open("note.txt", ...)` is assigned descriptor **`3`**. After `close(3)` releases slot `3`, the second `open("note.txt", O_RDONLY)` immediately reuses slot **`3`**.

---

## 4. Part 3: Counting One Kind of System Call

### 4.1 Measuring `write` and `openat` Counts
Using `strace` with `-e trace=<syscall>` and redirecting program `stdout` to `/dev/null` while piping `strace`'s `stderr` (`2>&1 >/dev/null | grep -c`) prevents the program's own output (such as the line `"write sent 14 bytes"`) from interfering with `grep`:

```bash
$ strace -f -e trace=write ls -l 2>&1 >/dev/null | grep -c '^write'
1

$ strace -f -e trace=openat ls -l 2>&1 >/dev/null | grep -c '^openat'
68

$ strace -e trace=write ./syscalls 2>&1 >/dev/null | grep -c '^write'
2
```

### 4.2 Summary and Buffering Analysis

| Command | Syscall Traced | Redirected (`>/dev/null`) | Interactive Terminal (TTY) | Reason for Difference |
| :--- | :--- | :---: | :---: | :--- |
| `ls -l` | `write` | `1` | `8` (header + 7 entries) | `libc` switches `stdout` from line-buffered (TTY) to block-buffered (`>/dev/null`), combining all lines into one `write(1, ...)`. |
| `ls -l` | `openat` | `68` (sandbox) / `30` (host) | `68` (sandbox) / `30` (host) | Dynamic linker, locale lookups, `.`, NSS/userdb lookups, and `/etc/localtime`. |
| `./syscalls` | `write` | `2` | `4` | On TTY: 3 line-buffered `printf` `write(1, ...)` calls + 1 direct `write(3, ...)`. Redirected: 3 `printf`s merge into 1 flushed `write(1, ..., 80)` + 1 direct `write(3, ...)`. |

Notice why `2>&1 >/dev/null` is essential when counting `./syscalls`: if `stdout` is piped directly into `grep -c '^write'` (`2>&1 | grep -c '^write'`), the program's own stdout line `"write sent 14 bytes"` enters the pipe and falsely matches `^write`, inflating the count from `2` to `3`. Redirecting `stdout` to `/dev/null` after duplicating `stderr` to the pipe (`2>&1 >/dev/null`) isolates the pure `strace` output on `stderr`.

---

## 5. Conclusions

1. **System Calls as the User–Kernel Gate:** Using `strace` revealed that even a routine command like `ls -l` performs roughly **250–260** system calls—spanning dynamic library loading (`openat`, `mmap`, `read`, `close`), identity/timezone configuration lookups, directory reading (`getdents64`, `statx`), and terminal output (`write`).
2. **Direct Kernel I/O vs. Standard I/O:** Implementing `syscalls.c` demonstrated how POSIX `open()`, `write()`, `read()`, and `close()` pass bytes directly across the user–kernel boundary, with `glibc`'s `open()` wrapper resolving to the Linux `openat(AT_FDCWD, ...)` system call.
3. **File Descriptor Allocation Discipline:** Descriptors `0`, `1`, and `2` are reserved for `stdin`, `stdout`, and `stderr`, so the first user-opened file receives descriptor `3`, which is deterministically recycled once closed.
4. **Impact of User-Space Buffering:** Comparing redirected runs (`2` `write` calls in `./syscalls`, `1` in `ls -l`) with terminal runs (`4` `write` calls in `./syscalls`, per-line writes in `ls -l`) confirmed how `libc` buffering minimizes expensive kernel context switches.

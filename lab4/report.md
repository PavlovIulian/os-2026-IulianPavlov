# Laboratory Work No. 4: The xv6 Shell from the Outside

**Course:** Operating Systems (Track B: xv6)  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Overview and Purpose
The user interface of xv6 is implemented as a single, compact user-space C program: `user/sh.c`, approximately 500 lines long. The objective of this laboratory is to operate the xv6 shell side-by-side with GNU Bash on Linux, investigating:
- The fundamental boundaries between user-space command interpretation and kernel-space process execution.
- Adapting existing user programs to upstream kernel system call evolution (`sleep()` to `pause()`).
- Core shell primitives implemented directly through POSIX-style system calls: `fork()`, `exec()`, `wait()`, `pipe()`, `open()`, `close()`, `dup()`, and `chdir()`.
- Argument passing mechanics: determining whether wildcards (`*`) and quote removal are performed by the program, the kernel, or the shell.
- The internal Abstract Syntax Tree (AST) architecture of `user/sh.c`.

---

## 2. Part 1: Fix Sleep for the Current xv6

### 2.1 The `sleep()` to `pause()` System Call Evolution
In August 2025, xv6 renamed the traditional user system call `sleep()` to `pause()` to resolve naming conflicts with the internal kernel synchronization primitive `sleep(void *chan, struct spinlock *lk)`.

Querying `user/user.h`:
```bash
xv6-riscv ❯ grep -n "pause\|sleep" user/user.h
25:int pause(int);
```
Line 25 declares `int pause(int);`. Thus, user programs must invoke `pause()` instead of `sleep()`.

### 2.2 Implementation of `user/sleep.c`
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
  pause(atoi(argv[1])); // was sleep() before Aug 2025
  exit(0);
}
```

### 2.3 Build Configuration and Verification
The executable is registered in the xv6 `Makefile` under `UPROGS`:
```makefile
UPROGS=\
	$U/_args\
	$U/_cat\
	$U/_echo\
	$U/_sleep\
    ...
```

Executing under QEMU (`make qemu`):
```bash
$ sleep 20 
$ sleep
usage: sleep <ticks>
$ 
```
- `sleep 20`: The process yields CPU execution for 20 timer ticks and cleanly returns.
- `sleep`: Emits the usage error to file descriptor 2 (stderr) and exits with code 1.

---

## 3. Part 2: bash versus the xv6 shell

### 3.1 Setup
A sandbox directory was created on Linux:
```bash
mkdir ~/shtest && cd ~/shtest && touch README
```
The exact same commands were evaluated concurrently in GNU Bash and in the xv6 shell.

### 3.2 Comparison Table

| Command | Tests | Bash Output | xv6 Output | Explanation |
| :--- | :--- | :--- | :--- | :--- |
| `echo hello > f ; cat < f` | redirection | `hello` | `hello` | Both shells support I/O redirection via `open()`, `dup()`, and `close()`. |
| `ls \| grep READ` | pipe | `README` | `README 2 2 2441` | Both shells use the `pipe()` system call to connect stdout of `ls` to stdin of `grep`. |
| `(echo a; echo b) \| wc` | grouping, list | `2 2 4` | `2 2 4` | Parentheses run a subshell list whose combined output is piped to `wc`. |
| `echo one ; echo two` | sequence | `one`<br>`two` | `one`<br>`two` | Semicolon `;` executes commands sequentially. |
| `echo bg &` | background | `[1] <PID>`<br>`bg` | `bg` | `&` forks a child process without the shell calling `wait()`. |
| `ls *` | globbing | `README` | `ls: cannot open *` | Bash performs pathname expansion (globbing). xv6 `sh.c` has no wildcard logic and passes literal `*`. |
| `echo $HOME` | variables | `/home/sunflower` | `$HOME` | Bash expands `$HOME`. xv6 has no environment variables. |
| `echo "a b"` | quotes | `a b` | `"a b"` | Bash strips quotes after preserving spaces. xv6 preserves quotes literally. |
| `mkdir d ; cd d ; ls ..` | PATH & CWD | Lists parent directory | `exec cd failed`<br>*(or `ls` fails)* | Bash searches `$PATH`. xv6 has no `$PATH`; binaries must be in CWD or referenced by absolute path (`/ls ..`). |
| `Up, Tab, Ctrl+R` | history, completion | History recall & completion | Literal escape codes (`^[[A`) | Bash uses GNU Readline. xv6 console driver reads raw characters without editing or history. |

---

## 4. Part 3: Who Expands `*`? Argument Printing Investigation

### 4.1 Programs Implemented

#### `user/args.c` (xv6)
```c
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    printf("argc = %d\n", argc);
    for(int i = 0; i < argc; i++)
        printf("argv[%d] = [%s]\n", i, argv[i]);
    exit(0);
}
```

#### `args_linux.c` (Linux)
```c
#include <stdio.h>

int
main(int argc, char *argv[])
{
    printf("argc = %d\n", argc);
    for(int i = 0; i < argc; i++)
        printf("argv[%d] = [%s]\n", i, argv[i]);
    return 0;
}
```

### 4.2 Test Executions for `args *.c "x y"`

#### Linux (Bash)
```bash
$ ./args *.c "x y"
argc = 27
argv[0] = [./args]
argv[1] = [args.c]
argv[2] = [cat.c]
argv[3] = [dorphan.c]
...
argv[24] = [wc.c]
argv[25] = [zombie.c]
argv[26] = [x y]
```

#### xv6 (sh)
```bash
$ args *.c "x y"
argc = 4
argv[0] = [args]
argv[1] = [*.c]
argv[2] = ["x]
argv[3] = [y"]
```

### 4.3 Analysis
1. **Who expands `*` and removes quotes?**  
   The **shell**. The target program merely reads the `argv` array passed to it. The operating system kernel merely copies `argv` into the process stack during `execve`. It is the shell that scans the command line, expands wildcard globs against directory files, strips quotes, and builds `argv` before executing `exec`.
2. **Handling of extra spaces in `one   two      three`:**  
   Both shells collapse multiple contiguous spaces between arguments during tokenization (word splitting), passing `[one]`, `[two]`, `[three]`.

---

## 5. Part 4: Architectural Code Walkthrough of `user/sh.c`

### 5.1 Command Classification
The entire xv6 shell comprises exactly **499 lines** (`wc -l user/sh.c`).

Defined command node types (`#define`):
```c
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5
#define MAXARGS 10
```

- `EXEC` (1): Regular command execution (words).
- `REDIR` (2): I/O redirection (`<`, `>`, `>>`/`+`).
- `PIPE` (3): Inter-process pipeline (`|`).
- `LIST` (4): Sequential execution (`;` and `(...)`).
- `BACK` (5): Background asynchronous task (`&`).

### 5.2 Why `cd` Cannot Be an External Program
In `user/sh.c` (lines 165–172):
```c
if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
  // Chdir must be called by the parent, not through runcmd!
  buf[strlen(buf)-1] = 0;  // chop \n
  if(chdir(buf+3) < 0)
    fprintf(2, "cannot cd %s\n", buf+3);
  continue;
}
```
If `cd` were a separate program, the shell would `fork()` a child process. The child would execute `chdir()`, changing only its own working directory, and terminate. The parent shell's working directory would remain unchanged. Therefore, `cd` must be executed within the main shell process.

### 5.3 Absence of Globbing and Variable Logic
Searching `user/sh.c` for `*` or `$` reveals no expansion logic. `gettoken()` only recognizes `< > | ; & ( )`. It has no filesystem pattern matching or environment variable lookup, directly explaining why `ls *` and `echo $HOME` print literal characters.

---

## 6. Answers to Laboratory Questions

### Part 2 Questions
1. **List the features both shells have, and the ones only bash has.**  
   - **Both have:** Input redirection (`<`), output redirection (`>`), append (`>>`), pipes (`|`), sequence (`;`), grouping (`(...)`), background (`&`), built-in `cd`.  
   - **Only Bash has:** Globbing (`*`, `?`, `[...]`), environment variables (`$VAR`), quote stripping (`"..."`, `'...'`), `$PATH` multi-directory resolution, command substitution (`$(...)`), arithmetic expansion (`$((...))`), command history, GNU Readline editing, tab completion, job control (`jobs`, `fg`, `bg`).
2. **In xv6, `ls` stops working after `cd d`. Why? Find a way to run `ls` from inside `d` anyway.**  
   xv6 has no `$PATH`. The shell passes `ls` directly to `exec("ls", argv)`, which searches relative to current working directory (`d/ls`). Inside `d`, `ls` does not exist.  
   *Fix:* Run via absolute path `/ls ..` or parent-relative path `../ls ..`.
3. **Which missing feature would you miss most in daily work, and which would be easiest to add?**  
   - *Most missed:* `$PATH` lookup (or tab completion / command history).  
   - *Easiest to add:* A default fallback `$PATH` mechanism in `user/sh.c`: if `exec(cmd, argv)` fails, prepend `/` and retry with `exec("/" + cmd, argv)` (<10 lines of C).

### Part 3 Questions
1. **Paste both outputs for `args *.c "x y"`. How many arguments did each program receive?**  
   - Linux (Bash): 27 arguments (`argc = 27`). `argv[1..25]` were expanded `.c` files, `argv[26]` was `[x y]`.  
   - xv6 (sh): 4 arguments (`argc = 4`). `argv[1] = [*.c]`, `argv[2] = ["x]`, `argv[3] = [y"]`.
2. **So who turns `*.c` into file names and removes the quotes: the program, the kernel, or the shell?**  
   The **shell**. The program only prints what it receives in `argv`. The kernel only copies the pointer array across address spaces during `execve`. The shell parses the line, matches directory files, strips quotes, and builds `argv`.
3. **What happened to the extra spaces in `one   two      three`?**  
   The extra spaces were discarded during the shell's tokenization phase (word splitting).

### Part 4 Questions
1. **Name the five kinds of command the xv6 shell knows and the character that produces each.**  
   1. `EXEC`: Basic executable command (words).  
   2. `REDIR`: Redirection (`<`, `>`, `>>`/`+`).  
   3. `PIPE`: Pipeline (`|`).  
   4. `LIST`: Sequence (`;` and `(...)`).  
   5. `BACK`: Background (`&`).
2. **Where is `cd` handled, and why is it not run as a separate program?**  
   Handled in `user/sh.c` in `main()` before `fork()`. Running it in a child process would only change the child's working directory, leaving the parent shell unaffected.
3. **Search `sh.c` for any code that handles `*` or `$`. What does that explain about Part 2?**  
   There is zero code handling `*` or `$`. This explains why `ls *` tried to open a file named `*`, and `echo $HOME` printed literal `$HOME`.

---

## 7. Conclusions
1. **Minimalist UNIX Design:** A fully functional shell supporting redirection, pipelines, lists, subshells, and background execution can be implemented in fewer than 500 lines of C when built atop orthogonal system calls (`fork`, `exec`, `pipe`, `open`, `close`, `dup`).
2. **Clear Division of Roles:** Wildcard expansion and quote stripping are purely user-space responsibilities performed by the shell before invoking the kernel.
3. **Process Isolation:** The necessity of `cd` as a built-in highlights process address space isolation: child processes cannot alter parent process state.
4. **API Evolution:** Updating `sleep.c` to use `pause()` demonstrated user-space adaptation to evolving kernel interfaces.
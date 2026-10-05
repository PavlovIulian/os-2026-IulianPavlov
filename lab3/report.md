# Laboratory Work No. 3: The Command Line: Shell, Pipes and Text Filters

**Course:** Operating Systems (Track A: Linux)  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Zaica Maia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Laboratory Objectives
The primary objective of this laboratory is to achieve command-line proficiency and mastery over the UNIX/Linux execution environment and GNU Bash shell. Key competencies covered include:
- Understanding shell execution mechanics: internal built-ins vs. external binaries on the filesystem.
- Resolving executable paths via the `$PATH` environment variable and inspecting process exit statuses (`$?`).
- Analyzing shell expansion stages: pathname expansion (globbing), brace expansion, parameter/variable expansion, command substitution, arithmetic evaluation, and word splitting.
- Managing standard POSIX I/O streams (`stdin`, `stdout`, `stderr`) via redirection operators (`>`, `>>`, `<`, `2>`, `2>&1`).
- Chaining commands sequentially (`;`) and conditionally (`&&`, `||`).
- Managing background tasks and process lifecycle (`&`, `jobs`, `kill`).
- Constructing high-performance data processing pipelines using composable UNIX text filters (`grep`, `cut`, `sort`, `uniq`, `awk`, `wc`, `tee`).
- Implementing robust shell automation scripts (`report.sh`) with input validation, stream routing, non-zero exit codes, and automated periodic scheduling via `cron`.

---

## 2. Part 1: Move Fast, Find Help

### 2.1 Tab Autocompletion and History Navigation
- **Command:** `ls /usr/sh` + **Tab twice**  
  *Result:* The shell queries candidate files and directories starting with `sh` inside `/usr/`. Because 227 possibilities exist, Bash displays a prompt listing all candidates (e.g., `share/`).
- **Command:** `cd /et` + **Tab once**  
  *Result:* The shell unambiguously autocompletes the line to `cd /etc/` and appends the trailing directory separator.
- **Command:** `Ctrl+R`  
  *Result:* Enters Readline's reverse-i-search mode, allowing instant matching and recall of previously executed commands by typing substring matches.

### 2.2 Shell Built-ins, External Binaries, and `$PATH` Resolution
```bash
~ ❯ type cd ; type ls ; type echo ; which ls
cd is aliased to `zd'
ls is aliased to `eza -lh --group-directories-first --icons=auto'
echo is a shell builtin
/usr/bin/ls
```

- `echo`: Identified as a shell built-in.
- `ls`: Located on the filesystem at `/usr/bin/ls` (aliased in the user environment to `eza`).
- `cd`: Aliased to `zd` (zoxide directory jumper); underlying `cd` is a shell built-in.

#### PATH Breakdown
Inspecting the `$PATH` search variable by replacing `:` delimiters with newlines:
```bash
~ ❯ echo $PATH | tr ':' '\n'
/home/sunflower/.local/share/mise/installs/claude/latest
/home/sunflower/.local/share/mise/installs/codex/latest/bin
/home/sunflower/.local/share/mise/installs/gemini/latest/node_modules/.bin
/home/sunflower/.local/share/mise/installs/gh/latest/gh_2.102.0_linux_amd64/bin
/home/sunflower/.local/share/mise/installs/node/26.8.2/bin
/home/sunflower/.local/share/mise/shims
/usr/share/omarchy/bin
/usr/local/sbin
/usr/local/bin
/usr/bin
/home/sunflower/.local/bin
/usr/bin/site_perl
/usr/bin/vendor_perl
/usr/bin/core_perl
/opt/010editor
```
There are **15 directories** in `$PATH`. The `ls` executable is found inside `/usr/bin`.

#### Exit Status on Command Not Found
```bash
~ ❯ nosuchprog ; echo "exit code: $?"
bash: command not found: nosuchprog
exit code: 127
```
When an executable cannot be found in `$PATH`, the shell exits with status code **127**.

### 2.3 Documentation and Manuals
- `man ls`: Manual pager for `ls`. The `-t` option sorts entries by modification time (newest first).
- `man -k "list directory"`: Searches manual summaries for keywords (`dir (1)`, `ls (1)`, `vdir (1)`).
- `help cd`: Built-in documentation for the internal `cd` command.

---

## 3. Part 2: What the Shell Does to Your Line

### 3.1 Setup
A sandbox directory was created with deliberate test files:
```bash
mkdir -p ~/os-lab3/play && cd ~/os-lab3/play
touch a.c. b.c file1.txt file2.txt notes.txt "alfa beta.txt"
```

### 3.2 Predictions vs. Real Output

| Command | My Prediction | Real Output | Technical Explanation |
| :--- | :--- | :--- | :--- |
| `echo *.c` | `b.c` | `b.c` | Matches filenames ending in `.c`. Since `a.c.` has a trailing dot, only `b.c` matched the glob pattern. |
| `echo file?.txt` | `filetxt1.txt filetxt2.txt` | `file1.txt file2.txt` | The `?` wildcard matches exactly one arbitrary character, matching `file1.txt` and `file2.txt`. |
| `echo [a-b]*` | `alfabeta.txt` | `a.c. alfa beta.txt b.c` | Character class `[a-b]` matches any file whose name begins with 'a' or 'b', matching all three files. |
| `echo *.xyz` | `*.xyz` | `*.xyz` | When no directory entries match a glob pattern, Bash leaves the pattern unexpanded as a literal string. |
| `ls alfa beta.txt` | `error` | `"alfa": No such file`<br>`"beta.txt": No such file` | Unquoted spaces are treated as argument delimiters (word splitting). The shell looks for two separate files. |
| `ls "alfa beta.txt"` | `home/sunflower` | `'alfa beta.txt'` | Double quotes prevent word splitting, passing the full filename containing the space as a single token. |
| `echo '$HOME' "$HOME" \$HOME` | `$HOME /home/sunflower $HOME` | `$HOME /home/sunflower $HOME` | Single quotes suppress variable expansion; double quotes expand `$HOME`; backslash escapes the `$`. |
| `echo {1..5} ; echo $((7 * 6))` | `1 2 3 4 5`<br>`42` | `1 2 3 4 5`<br>`42` | `{1..5}` undergoes brace expansion; `$((7 * 6))` performs arithmetic expansion evaluated by bash. |
| `echo "today is $(date +%A), I am $(whoami)"` | `today is Monday i am sunflower` | `today is Monday, I am sunflower` | Command substitution `$(...)` executes the enclosed command and embeds its stdout into the string. |
| `touch log{1..3}.txt && ls log*` | `logs log1 log2 log3` | `log1.txt log2.txt log3.txt` | Brace expansion generates `log1.txt log2.txt log3.txt`; subsequent globbing matches and lists them. |

---

## 4. Part 3: Redirection, Chaining and Jobs

### 4.1 Stream Separation
```bash
ls /etc /nope > out.txt 2> err.txt
wc -l out.txt ; cat err.txt
```
- `out.txt` receives standard output (file descriptor 1): the directory listing of `/etc`.
- `err.txt` receives standard error (file descriptor 2): the error diagnostic `"nope": No such file or directory`.

### 4.2 Order of Redirections: `> all.txt 2>&1` vs. `2>&1 > all.txt`
- `ls /etc /nope > all.txt 2>&1`: Evaluated left-to-right. First, stdout (FD 1) is redirected to `all.txt`. Then, stderr (FD 2) is duplicated to FD 1 (which now points to `all.txt`). Both stdout and stderr are written into `all.txt`.
- `ls /etc /nope 2>&1 > all.txt`: Evaluated left-to-right. First, stderr (FD 2) is duplicated to FD 1. Because FD 1 still points to the terminal screen, FD 2 is directed to the screen. Then, stdout (FD 1) is redirected to `all.txt`. The directory listing goes to `all.txt`, while the error message is displayed on the screen.

### 4.3 Truncation vs. Appending
```bash
echo first > notes.md ; echo second >> notes.md ; cat notes.md
```
`>` truncates (overwrites) existing contents, while `>>` appends new content to the end of the file.

### 4.4 File Argument vs. Stdin Redirection
- `wc -l access.log` prints `500 access.log` because `wc` receives the filename in `argv` and prints it for clarity.
- `wc -l < access.log` prints only `500` because the shell attaches the file to `stdin` (FD 0); `wc` reads from an anonymous stream and does not know the filename.

### 4.5 Command Chaining and Job Control
- `;`: Executes unconditionally in sequence.
- `&&`: Executes the right-hand command only if the left-hand command succeeds (exit code 0).
- `||`: Executes the right-hand command only if the left-hand command fails (non-zero exit code).
```bash
ls /nope 2> /dev/null || echo "failed with code $?"
# Output: failed with code 2
```
Background job management:
```bash
sleep 100 &
jobs
kill %1
jobs
```
Spawns job `1` in the background, displays running state with `jobs`, sends `SIGTERM` with `kill %1`, and verifies process termination.

---

## 5. Part 4: Answer Questions with One-Liners

### Dataset Summary
An artificial access log of 500 requests with format:
`2026-10-05 10:XX userN STATUS /pageM`

### One-Liner Solutions

#### Q1. How many requests failed?
```bash
grep -c FAIL access.log
```
**Output:** `100`

#### Q2. How many different pages were requested, and which?
```bash
cut -d' ' -f5 access.log | sort -u | tee >(wc -l)
```
**Output:**
```
/page0
/page1
/page2
/page3
4
```

#### Q3. How many requests did each page get?
```bash
cut -d' ' -f5 access.log | sort | uniq -c
```
**Output:**
```
    125 /page0
    125 /page1
    125 /page2
    125 /page3
```
*Verification:* $125 \times 4 = 500$ requests total.

#### Q4. Which user or users have the most failed requests, and how many?
```bash
grep FAIL access.log | cut -d' ' -f3 | sort | uniq -c | sort -rn | awk 'NR==1{m=$1} $1==m'
```
**Output:**
```
     15 user5
     15 user3
```

#### Q5. How many requests did user3 make, and how many of them failed?
```bash
grep ' user3 ' access.log | cut -d' ' -f4 | sort | uniq -c
```
**Output:**
```
     15 FAIL
     57 OK
```
*Verification:* $15 + 57 = 72$ total requests made by user3.

#### Q6. Print the last 3 failed requests, showing only time and user.
```bash
grep FAIL access.log | tail -3 | cut -d' ' -f2,3
```
**Output:**
```
10:10 user0
10:15 user5
10:20 user3
```

#### Q7. Which login shells appear in `/etc/passwd`, and how many accounts use each?
```bash
cut -d: -f7 /etc/passwd | sort | uniq -c | sort -rn
```
**Output:**
```
     26 /usr/bin/nologin
      3 /usr/bin/bash
      1 /usr/bin/git-shell
```

#### Q8. Why do `ls /etc | wc -l` and `ls -l /etc | wc -l` differ by exactly one?
```bash
command ls /etc | wc -l ; command ls -l /etc | wc -l
```
**Output:**
```
192
193
```
**Explanation:** When stdout is connected to a pipe, standard `ls` outputs exactly one filename per line (192 files). `ls -l` outputs an introductory summary line specifying total block allocation (e.g., `total 1024`), followed by the 192 file entries, resulting in $192 + 1 = 193$ lines.

### Bonus Task (+1): Extended Regular Expressions
Finding failed requests where the minute ends in 0 (10:00, 10:10, ...):
```bash
grep -E ' 10:[0-5]0 .*FAIL' access.log
grep -E -c ' 10:[0-5]0 .*FAIL' access.log
```
**Output:** `50` occurrences.

---

## 6. Part 5: Your First Script and Cron Automation

### 6.1 Script Implementation (`report.sh`)
```bash
#!/bin/bash
# usage: ./report.sh access.log
log=$1
if [ ! -f "$log" ]; then
    echo "no such file: $log" >&2
    exit 1
fi
total=$(wc -l < "$log")
fails=$(grep -c FAIL "$log")
echo "$total requests, $fails failed"
for n in 0 1 2 3 4 5 6; do
    c=$(grep " user$n " "$log" | grep -c FAIL)
    echo "user$n: $c failures"
done
```

### 6.2 Execution and Testing
```bash
chmod +x report.sh
./report.sh access.log
```
**Output:**
```
500 requests, 100 failed
user0: 14 failures
user1: 14 failures
user2: 14 failures
user3: 15 failures
user4: 14 failures
user5: 15 failures
user6: 14 failures
```
*Arithmetic Check:* $14 \times 5 + 15 \times 2 = 70 + 30 = 100$ failures.

#### Error Handling Test
```bash
./report.sh nope.log ; echo "exit code: $?"
```
**Output:**
```
no such file: nope.log
exit code: 1
```

### 6.3 Cron Scheduling (Bonus +1)
Crontab entry configured via `crontab -e`:
```cron
* * * * * /home/sunflower/os-lab3/report.sh /home/sunflower/os-lab3/access.log >> /home/sunflower/os-lab3/report.log 2>&1
```
Verified through `crontab -l`, waited for 130 seconds, observed repeated automated reports appended into `~/os-lab3/report.log`, and cleared with `crontab -r`.

---

## 7. Answers to Laboratory Questions

### Part 1
1. **Which of `cd`, `ls`, `echo` are built-ins, and why MUST `cd` be one?**  
   `cd` and `echo` are built-ins; `ls` is an external binary (`/usr/bin/ls`). `cd` must be a built-in because in the UNIX process model, external commands run inside a forked child process. A child process cannot modify the working directory of its parent process; if `cd` were external, the parent shell's working directory would remain unchanged.
2. **How many directories are in your PATH, and in which one is `ls`?**  
   There are **15 directories** in `$PATH`. `ls` is located in `/usr/bin`.
3. **What exit code does a command that is not found return?**  
   It returns exit code **127**.
4. **Which option of `ls` sorts by modification time?**  
   The `-t` option (or `--sort=time` in GNU coreutils).

### Part 2
1. **Predictions vs. outputs:** See Table in Section 3.2.
2. **`echo` knows nothing about files, yet `echo *.c` printed file names. Who produced them?**  
   The shell (Bash) produced them via pathname expansion (globbing) *before* executing `echo`. The matched filenames were passed as discrete elements in `argv`.
3. **Why did `ls alfa beta.txt` fail? Give two different ways to fix it.**  
   Unquoted spaces are treated as argument delimiters (word splitting). The shell searched for two files: `alfa` and `beta.txt`.  
   *Fix 1:* Quote the argument: `ls "alfa beta.txt"` or `ls 'alfa beta.txt'`.  
   *Fix 2:* Escape the space: `ls alfa\ beta.txt`.
4. **What is the difference between single and double quotes?**  
   Single quotes preserve the literal value of all enclosed characters, suppressing all expansions. Double quotes allow variable expansion (`$VAR`), command substitution (`$(...)`), and arithmetic expansion (`$((...))`), while suppressing word splitting and globbing. `ls` wraps filenames containing spaces in quotes to clarify that the space is part of the filename.

### Part 3
1. **Which file got the error message, and which got the listing?**  
   `err.txt` received the error message (FD 2). `out.txt` received the directory listing (FD 1).
2. **Difference between `> all.txt 2>&1` and `2>&1 > all.txt`:**  
   Redirections are evaluated left to right. In `> all.txt 2>&1`, stdout is redirected to `all.txt`, and stderr is duplicated to point to the same file. In `2>&1 > all.txt`, stderr is first duplicated to point to the current destination of stdout (the screen), and stdout is then redirected to `all.txt`, leaving stderr directed to the screen.
3. **Why does `wc -l < access.log` print only a number, while `wc -l access.log` also prints the filename?**  
   When passed as an argument, `wc` reads the filename from `argv` and prints it. When redirected via stdin, the shell connects the file to FD 0; `wc` reads from an anonymous stream without access to the filename.
4. **Explain each operator in one sentence:**  
   - `;`: Executes commands sequentially regardless of exit status.
   - `&&`: Executes the right-hand command only if the left-hand command succeeds with exit code 0.
   - `||`: Executes the right-hand command only if the left-hand command fails with a non-zero exit code.

### Part 4 (Q8)
- **Why do `ls /etc | wc -l` and `ls -l /etc | wc -l` differ by exactly one?**  
  Plain `ls` when piped outputs one entry per line. `ls -l` outputs an initial block allocation summary line (`total <blocks>`) before the file entries, making its output exactly 1 line longer.

### Part 5
1. **What does the first line `#!/bin/bash` do, and what happens if you run `./report.sh` before `chmod +x`?**  
   The shebang (`#!`) instructs the operating system kernel which interpreter to invoke to execute the script. Running the script without execute permissions (`+x`) causes the kernel to reject execution with `Permission denied` (exit status 126).
2. **Why must you type `./report.sh` and not just `report.sh`?**  
   For security reasons, the current directory (`.`) is excluded from `$PATH`. An explicit relative path (`./`) must be provided to locate and execute files in the local directory.
3. **Why does the script write the error with `>&2` and end with `exit 1`?**  
   `>&2` routes error messages to standard error to prevent corrupting standard output streams. `exit 1` signals a non-zero exit code indicating failure to calling processes and automation scripts.

---

## 8. Conclusions
In this laboratory work, the fundamental operational model of the UNIX shell, standard I/O streams, command chaining, stream filtering, and shell automation were examined and empirically validated. 

By analyzing differences between predictions and actual command outcomes, the exact order of shell expansions (brace expansion, word splitting, pathname expansion, and quote handling) was mapped. The UNIX philosophy of combining small, specialized filters (`grep`, `cut`, `sort`, `uniq`, `awk`, `wc`) via pipes was proven through single-line data extraction pipelines across hundreds of log records. Finally, automating the analysis with a shell script and integrating it into the `cron` daemon illustrated how command-line primitives scale into production-grade systems management.
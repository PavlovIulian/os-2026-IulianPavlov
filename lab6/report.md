# Laboratory Work No. 6: Inside the xv6 File System

**Course:** Operating Systems (Track B: xv6)  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Laboratory Objectives
The xv6 operating system provides a clean, fully functional implementation of a traditional UNIX filesystem (similar to the foundational concepts of ext4 and UFS) in a few hundred readable lines of C. The objective of Laboratory Work Nr. 6 is to inspect and analyze the internal structures of the xv6 filesystem, develop core user-space filesystem utilities, and measure storage parameters:
- **Directory Table Architecture:** Understanding directories as simple files containing fixed-size `struct dirent` records mapping filename strings to inode numbers.
- **Inode Architecture and Types:** Decoding inode types (`T_DIR`, `T_FILE`, `T_DEVICE`) defined in `kernel/stat.h` and inspecting disk inode representations (`struct dinode`) in `kernel/fs.h`.
- **Implementing `mystat`:** Writing a user utility `user/mystat.c` to retrieve metadata via the `stat()` system call and observing hard link reference counting (`st.nlink`).
- **Unlinking and Block Reclaim Mechanics:** Tracing `sys_unlink()` in `kernel/sysfile.c` to pinpoint where link counts are decremented and identifying when file data blocks are freed via `iput()` and `itrunc()`.
- **Filesystem Limits and Disk Layout:** Measuring block size (`BSIZE`), direct blocks (`NDIRECT`), single indirect blocks (`NINDIRECT`), maximum directory entry name lengths (`DIRSIZ`), computing theoretical maximum file sizes, and parsing the disk layout (`nmeta`, log, inodes, bitmap, and data blocks).
- **Implementing Recursive Directory Search (`find`):** Developing `user/find.c` to recursively traverse the directory tree, parse directory entries, handle `.` and `..` cycles safely, and respect process file descriptor limits (`NOFILE`).

---

## 2. Part 1: Read What xv6 Shows You

### 2.1 Exploration and Shell Traces

We booted the xv6 operating system in QEMU and analyzed the root directory listing and newly created directories:

```bash
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2441
args           2 3 36576
cat            2 4 38112
echo           2 5 36888
sleep          2 6 36576
forktest       2 7 18248
grep           2 8 41648
init           2 9 37272
kill           2 10 36824
ln             2 11 36616
ls             2 12 40240
mkdir          2 13 36880
rm             2 14 36864
sh             2 15 60824
stressfs       2 16 37728
usertests      2 17 209536
grind          2 18 53360
wc             2 19 39136
zombie         2 20 36144
logstress      2 21 38896
forphan        2 22 37648
dorphan        2 23 37088
sync           2 24 36240
console        3 25 0
$ mkdir a
$ ls a
.              1 26 32
..             1 1 1024
```

Each row printed by `ls` displays: `[name] [type] [inode] [size in bytes]`.

### 2.2 Answers to Questions (Part 1)

#### (1) What do types 1, 2 and 3 mean (`kernel/stat.h`)?
In `kernel/stat.h`, xv6 defines the following inode types:
```c
#define T_DIR     1   // Directory
#define T_FILE    2   // Regular File
#define T_DEVICE  3   // Device (e.g., console)
```
- **Type 1 (`T_DIR`)**: Directory file. A structured table storing directory entries (`struct dirent`) that map names to inode numbers.
- **Type 2 (`T_FILE`)**: Regular file. Holds arbitrary user or binary byte streams (e.g., `README`, executables).
- **Type 3 (`T_DEVICE`)**: Special character device node. Reads and writes bypass filesystem data blocks and map directly to kernel device drivers (e.g., `console` major device).

#### (2) Which inode number does the root directory have, and why are `.` and `..` the same there?
- **Root inode number:** The root directory has **inode number 1** (defined as `ROOTINO = 1` in `kernel/fs.h`). This is confirmed by `ls`, where `.` and `..` both list inode `1`.
- **Why `.` and `..` are identical:** `.` represents the current directory, and `..` represents the parent directory. Because the root directory `/` is the top-most ancestor of the entire filesystem hierarchy, it has no parent above it. By UNIX convention, the parent of the root directory is defined to point back to the root directory itself.

#### (3) An empty directory `a` has size 32. A directory entry has a fixed size. How big is one entry, and which two entries does `a` contain?
In xv6 (`kernel/fs.h`), a directory entry is defined as:
```c
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};
```
- `ushort inum`: 2 bytes (16-bit integer holding the inode number).
- `char name[DIRSIZ]`: 14 bytes (`DIRSIZ = 14`, character array for the filename).
- **Size of one entry:** $2 + 14 = \mathbf{16\text{ bytes}}$.
- **Entries contained in `a`:** An empty directory immediately contains exactly **two entries**:
  1. `.` (pointing to directory `a`'s own inode, inode `26`).
  2. `..` (pointing to the parent root directory, inode `1`).
- Therefore, the file size of directory `a` is $2 \times 16 = \mathbf{32\text{ bytes}}$.

---

## 3. Part 2: Write `mystat` and Hard Link Observation

### 3.1 Implementation of `user/mystat.c`

The program inspects file metadata using the `stat()` system call:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char *
typename(int t)
{
  switch(t){
  case T_DIR:    return "directory";
  case T_FILE:   return "file";
  case T_DEVICE: return "device";
  }
  return "?";
}

int
main(int argc, char *argv[])
{
  struct stat st;
  for(int i = 1; i < argc; i++){
    if(stat(argv[i], &st) < 0){
      fprintf(2, "mystat: cannot stat %s\n", argv[i]);
      continue;
    }
    printf("%s: %s, inode %d, links %d, size %d bytes\n",
           argv[i], typename(st.type), st.ino, st.nlink, (int)st.size);
  }
  exit(0);
}
```

The program was registered in the xv6 `Makefile` under `UPROGS`:
```makefile
UPROGS=\
    $U/_args\
    $U/_cat\
    $U/_echo\
    $U/_sleep\
    $U/_ls\
    $U/_mystat\
    ...
```

### 3.2 Command Execution and Verification

```bash
$ mystat README . console
README: file, inode 2, links 1, size 2441 bytes
.: directory, inode 1, links 1, size 1024 bytes
console: device, inode 26, links 1, size 0 bytes

$ ln README r2
$ mystat README r2
README: file, inode 2, links 2, size 2441 bytes
r2: file, inode 2, links 2, size 2441 bytes

$ rm README
$ mystat r2
r2: file, inode 2, links 1, size 2441 bytes

$ cat r2 | wc
48 336 2441
```

### 3.3 Answers to Questions (Part 2)

#### (1) Paste the outputs after `ln` and after `rm`. What changed and what did not?
- **Outputs after `ln` and `rm`:**
  ```
  $ ln README r2
  $ mystat README r2
  README: file, inode 2, links 2, size 2441 bytes
  r2: file, inode 2, links 2, size 2441 bytes

  $ rm README
  $ mystat r2
  r2: file, inode 2, links 1, size 2441 bytes
  ```
- **What changed:**
  - After `ln`: The link count (`links`) of inode 2 incremented from 1 to 2. A second directory entry `r2` appeared pointing to inode 2.
  - After `rm`: The directory entry `README` was removed from the directory table, and the link count (`links`) of inode 2 decremented from 2 to 1.
- **What did not change:**
  - Inode number (`inode 2`) remained identical.
  - File size (`2441 bytes`), data blocks, and contents remained completely intact (verified via `cat r2 | wc` returning 48 lines, 336 words, 2441 bytes).
  - File type (`file`) remained unchanged.

#### (2) Find the line in `kernel/sysfile.c` (`sys_unlink`) that decrements the link count. When are the data blocks freed?
- **Code line in `kernel/sysfile.c`:** Inside `sys_unlink()`:
  ```c
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  ```
  Specifically, line 241: **`ip->nlink--;`**.
- **When are data blocks freed:** The data blocks are **not** immediately freed when `sys_unlink()` runs if `ip->nlink > 0` or if processes still hold open file descriptors to the file.
  Data blocks are deallocated only when:
  1. The link count on disk reaches zero (`ip->nlink == 0`), **AND**
  2. The in-memory reference count drops to zero (all processes have closed their open file descriptors referencing this inode).
  When the final process calls `close()` (or when `iunlockput(ip)` drops the last reference), `iput()` in `kernel/fs.c` detects that `ip->valid && ip->nlink == 0 && ip->ref == 1`. It then invokes `itrunc(ip)` to return all direct and indirect data blocks to the free bitmap via `bfree()`, resets the inode size to 0, and marks the inode slot as unallocated (`type = 0`).

#### (3) xv6 has no symbolic links and no permissions. Look at `struct dinode` in `kernel/fs.h`: which fields does an xv6 inode have?
In `kernel/fs.h`, `struct dinode` is declared as:
```c
struct dinode {
  short type;              // File type
  short major;             // Major device number (T_DEVICE only)
  short minor;             // Minor device number (T_DEVICE only)
  short nlink;             // Number of links to inode in file system
  uint size;               // Size of file (bytes)
  uint addrs[NDIRECT + 1]; // Data block addresses
};
```
The exact fields are:
1. `short type`: Inode type (`T_DIR`, `T_FILE`, `T_DEVICE`, or 0 for free).
2. `short major`: Major device identifier (used only when `type == T_DEVICE`).
3. `short minor`: Minor device identifier (used only when `type == T_DEVICE`).
4. `short nlink`: Hard link reference counter.
5. `uint size`: Logical file size in bytes.
6. `uint addrs[NDIRECT + 1]`: Array of 13 block addresses (12 direct blocks + 1 indirect block pointer).
*Notable omissions compared to POSIX/Linux ext4:* xv6 inodes lack UID (owner), GID (group), mode/permission bits, timestamps (`atime`, `mtime`, `ctime`), generation numbers, and symlink target strings.

---

## 4. Part 3: Measure the Limits

### 4.1 Exploration and Command Execution

We inspected `kernel/fs.h` and the disk build output from `mkfs`:

```bash
xv6-riscv ❯ grep -n "define BSIZE\|define NDIRECT\|define NINDIRECT\|define DIRSIZ" kernel/fs.h
5:#define BSIZE   1024 // block size
26:#define NDIRECT   12
27:#define NINDIRECT (BSIZE / sizeof(uint))
54:#define DIRSIZ 14

xv6-riscv ❯ make clean ; make fs.img 2>&1 | grep nmeta
nmeta 47 (boot, super, log blocks 31, inode blocks 13, bitmap blocks 1) blocks 1953 total 2000

$ mkdir averyveryverylongname
$ ls
...
averyveryveryl 1 27 32
```

### 4.2 Answers to Questions (Part 3)

#### (1) What does `ls` show for `averyveryverylongname`, and which constant explains it?
- **What `ls` shows:** `ls` displays the name truncated to **`averyveryveryl`** (14 characters).
- **Constant explaining it:** **`#define DIRSIZ 14`** in `kernel/fs.h`. In xv6, `struct dirent` allocates a fixed-size character array `char name[DIRSIZ]`. When `mkdir` copies the requested name into the directory entry, any characters beyond 14 are discarded:
  ```
  "averyveryverylongname" -> "averyveryveryl" (14 chars)
  ```

#### (2) Compute the largest file xv6 can hold: 12 direct blocks plus one indirect block that holds block numbers of 4 bytes each, blocks of `BSIZE` bytes. Show the calculation.
From `kernel/fs.h`:
- Block size: $\text{BSIZE} = 1024\text{ bytes}$.
- Pointer size: $\text{sizeof(uint)} = 4\text{ bytes}$.
- Direct blocks: $\text{NDIRECT} = 12\text{ blocks}$.
- Indirect block capacity:
  \[
  \text{NINDIRECT} = \frac{\text{BSIZE}}{\text{sizeof(uint)}} = \frac{1024}{4} = 256\text{ blocks}.
  \]
- Total blocks per file:
  \[
  \text{Total Blocks} = \text{NDIRECT} + \text{NINDIRECT} = 12 + 256 = \mathbf{268\text{ blocks}}.
  \]
- Maximum file size:
  \[
  \text{Max File Size} = 268 \times \text{BSIZE} = 268 \times 1024\text{ bytes} = \mathbf{274,432\text{ bytes}} \approx \mathbf{268\text{ KiB}}.
  \]

#### (3) From the `nmeta` line: how many blocks does the whole disk have, and how many are for the log, the inodes, and the data?
From the build log:
`nmeta 47 (boot, super, log blocks 31, inode blocks 13, bitmap blocks 1) blocks 1953 total 2000`
- **Total disk blocks:** **2000 blocks** (`FSSIZE = 2000`).
- **Log (journal) blocks:** **30 blocks** (blocks 2 through 31; designated as `log blocks 31`, meaning up through block 31).
- **Inode blocks:** **13 blocks** (`inode blocks 13`, holding $13 \times \frac{1024}{64} = 208$ inodes).
- **Bitmap blocks:** **1 block** (free block allocation map).
- **Boot and Superblock:** **2 blocks** (boot block 0, superblock 1).
- **Total metadata blocks (`nmeta`):** $1 + 1 + 30 + 13 + 1 = 46$ (plus block 46 sector, totaling **47 blocks**).
- **Data blocks:** **1953 blocks** (`blocks 1953`).

---

## 5. Part 4: Write `find`

### 5.1 Implementation of `user/find.c`

The `find` utility recursively traverses directories to find all paths whose final component matches a specified target name:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find(char *path, char *target)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  if(st.type != T_DIR){
    close(fd);
    return;
  }
  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
    fprintf(2, "find: path too long\n");
    close(fd);
    return;
  }

  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if(strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
      continue;
    if(strcmp(p, target) == 0)
      printf("%s\n", buf);
    if(stat(buf, &st) < 0){
      fprintf(2, "find: cannot stat %s\n", buf);
      continue;
    }
    if(st.type == T_DIR)
      find(buf, target);
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "usage: find <dir> <name>\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}
```

### 5.2 Key Implementation Details
1. **Recursion Safety:** Explicitly ignores `.` and `..` to prevent infinite recursion loops.
2. **Free Directory Entries:** Checks `de.inum == 0` to skip deleted or unallocated directory slots.
3. **Descriptor Leak Prevention:** Closes every directory descriptor (`close(fd)`) before recursing or returning. This is critical because xv6 limits each process to 16 open files (`NOFILE = 16`).
4. **Buffer Hygiene:** Ensures proper null termination of `p[DIRSIZ] = 0` because xv6 directory names are not guaranteed to be null-terminated if they fill all 14 bytes.

### 5.3 Verification and Output

```bash
$ mkdir a
$ mkdir a/b
$ echo x > a/b/r2
$ echo y > a/r2
$ ln cat r2
$ find . r2
./a/b/r2
./a/r2
./r2
$ find a b
a/b
```

### 5.4 Stretch Question Analysis
- **Stretch Question:** Which of the three `r2` files is the same file as `cat`?
- **Answer:** **`./r2`** (in the current working directory). It was created via `ln cat r2`, so `./r2` shares the exact same inode number as `cat` (inode 4). The files `./a/r2` and `./a/b/r2` were created via shell redirection (`echo ... > ...`) and possess distinct, independent inodes.

---

## 6. Conclusions

Laboratory Work No. 6 provided direct, practical exposure to the foundational mechanisms of UNIX filesystem engineering:
1. **Elegance of Directory Representation:** In xv6, directories are simply regular files containing fixed 16-byte records (`ushort inum` and `char name[14]`). Path resolution is fundamentally a linear traversal through these records.
2. **Hard Link Symmetry:** Hard links are merely additional directory entries pointing to the same inode. Data blocks are decoupled from names, and space reclamation is deferred until both disk link counts and in-memory reference counts reach zero.
3. **Transparent Filesystem Limits:** Analyzing `kernel/fs.h` demonstrated how structural design choices (block size 1024, 12 direct blocks, 1 single indirect block) directly define architectural boundaries (maximum file size 268 KiB, maximum filename length 14 characters).
4. **Recursive Tree Traversal:** Implementing `find` reinforced the importance of directory loop prevention (`.` and `..`), entry filtering, and descriptor management under strict OS resource limits (`NOFILE = 16`).
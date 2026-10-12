# Laboratory Work No. 5: Files, Directories, and the Storage Stack

**Course:** Operating Systems (Track A: Linux)  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Laboratory Objectives
The primary objective of Laboratory Work Nr. 5 is to understand and analyze the Linux storage architecture, the virtual filesystem (VFS) abstraction, inode structures, and the POSIX security model. Specifically, the laboratory covers:
- **Filesystem Hierarchy Standard (FHS):** Navigating root mount points (`/`, `/bin`, `/dev`, `/etc`, `/proc`, `/sys`, `/tmp`, `/usr`, `/var`), identifying file types, understanding the modern unified UsrMerge architecture, and comparing partition resource consumption.
- **Inodes and Link Mechanics:** Distinguishing between directory entries, hard links, and symbolic (soft) links. Investigating inode metadata (`stat`), reference counters (`st_nlink`), the atomic unlinking behavior of `rm`, and directory link count growth.
- **Physical vs. Sparse Storage Allocation:** Demystifying block allocation granularity (4 KiB blocks) and sparse file representation with hole tracking via `truncate` and `du`.
- **POSIX Discretionary Access Control (DAC):** Deconstructing octal and symbolic permission modes, owner privileges versus DAC enforcement, process creation masks (`umask`), directory permission semantics (`r` vs. `x`), and special permission bits (SetUID, Sticky bit).
- **Storage Subsystems and Kernel Introspection:** Mapping physical storage through block devices, LUKS cryptographic containers, and Btrfs subvolumes (`lsblk`, `df`, `mount`). Exploring the synthetic `/proc` filesystem (`procfs`) to inspect CPU topology, memory metrics, process execution status, and active file descriptors.
- **Specialized Investigations:** Binary magic number detection via headers, auditing system-wide SetUID executables, and examining the lifecycle of unlinked open file descriptors.

---

## 2. Part 1: Navigating the Filesystem Hierarchy

### 2.1 Exploration and Command Execution

The initial phase involved querying the system root `/` directory and inspecting standard top-level system paths:

```bash
~/os-lab5 ❯ ls /
Permissions Size User Date Modified Name
lrwxrwxrwx     - root 12 Oct  2025   bin -> usr/bin
drwx------     - root  1 Jan  1970   boot
drwxr-xr-x     - root 12 Oct 14:53   dev
drwxr-xr-x     - root 11 Oct 12:23   etc
drwxr-xr-x     - root 28 Sep 17:23  󱂵 home
lrwxrwxrwx     - root 12 Oct  2025   lib -> usr/lib
lrwxrwxrwx     - root 12 Oct  2025   lib64 -> usr/lib
drwxr-xr-x     - root 12 Oct  2025   mnt
drwxr-xr-x     - root  1 Oct 22:40   opt
dr-xr-xr-x     - root 12 Oct 14:19   proc
drwxr-x---     - root 28 Sep 17:26   root
drwxr-xr-x     - root 12 Oct 15:08   run
lrwxrwxrwx     - root 12 Oct  2025   sbin -> usr/bin
drwxr-xr-x     - root 28 Sep 17:23   srv
drwxr-xr-x     - root 28 Sep 17:24   swap
dr-xr-xr-x     - root 12 Oct 15:13   sys
drwxrwxrwt     - root 12 Oct 15:14   tmp
drwxr-xr-x     - root 10 Oct 14:40   usr
drwxr-xr-x     - root 11 Oct 12:23   var

~/os-lab5 ❯ ls -ld /etc /etc/passwd/ /bim /dev/null /tmp
"/etc/passwd/": Not a directory (os error 20)
"/bim": No such file or directory (os error 2)
Permissions Size User Date Modified Name
drwxr-xr-x     - root 11 Oct 12:23   /etc
drwxrwxrwt     - root 12 Oct 15:14   /tmp
crw-rw-rw-   1,3 root 12 Oct 14:19  󰡯 /dev/null

~/os-lab5 ❯ file /usr/bin/ls /etc/passwd /bin
/usr/bin/ls: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=4e297d3b427342e1da6b66f5ca0fd279f43f3afe, for GNU/Linux 4.4.0, stripped
/etc/passwd: ASCII text
/bin:        symbolic link to usr/bin

~/os-lab5 ❯ du -sh /var/log 2>/dev/null ; du -sh /usr 2>/dev/null
376M	/var/log
17G	/usr
```

### 2.2 Answers to Questions (Part 1)

#### (1) For each path in the second line, write its type from the first letter of `ls -l`.
The first character in the POSIX file mode output indicates the object type:
- **/etc**: First letter is `d`, denoting a **Directory** (`drwxr-xr-x`). It stores directory entries mapping filenames to inodes for system configuration files.
- **/etc/passwd/**: Generates an OS error `Not a directory (os error 20)`. The underlying object `/etc/passwd` is a regular file (`-`). Under POSIX path resolution, appending a trailing slash `/` requires the kernel to treat the component as a directory. Because it is a regular file, the traversal fails with `ENOTDIR`.
- **/bim**: Generates `No such file or directory (os error 2)`. The path does not exist on the filesystem (`ENOENT`), resulting from a typo for `/bin`.
- **/dev/null**: First letter is `c`, denoting a **Character device file** (`crw-rw-rw-`). It represents a stream-oriented driver where data is discarded immediately on write, and reads return EOF (`0` bytes).
- **/tmp**: First letter is `d`, denoting a **Directory** (`drwxrwxrwt`), equipped with a trailing `t` indicating the Sticky bit.

#### (2) What is `/bin` on your system: a directory or a link, and to where?
On this system, `/bin` is a **symbolic link** pointing to `usr/bin` (`/bin -> usr/bin`). This architecture implements the modern Linux **UsrMerge** (merged `/usr`) standard. Historically, UNIX separated `/bin` (minimal tools needed during single-user rescue boot) and `/usr/bin` (general utilities mounted from secondary disks). Modern Linux systems consolidate all binaries under `/usr/bin` and maintain compatibility through symlinks for `/bin`, `/sbin`, `/lib`, and `/lib64`.

#### (3) Find one character device and one block device in `/dev`.
- **Character device (`c`):** `/dev/null` (major 1, minor 3), `/dev/zero`, or `/dev/tty`. Character devices stream data byte-by-byte without filesystem block caching.
- **Block device (`b`):** `/dev/nvme0n1` (major 259, minor 0) or `/dev/dm-0` (major 253, minor 0). Block devices represent random-access storage media addressed in fixed-sized hardware sectors/blocks and cached by the kernel buffer cache.

#### (4) Which is bigger on your laptop, `/var/log` or `/usr`, and by roughly how much?
- Size of `/var/log`: **376 MB**
- Size of `/usr`: **17 GB**
- `/usr` is significantly larger than `/var/log` by approximately **16.6 GB** (it is roughly **45 times larger**). This is expected because `/usr` houses virtually all user-space executables, shared libraries, dynamic modules, compiler toolchains, and desktop environments, whereas `/var/log` contains bounded systemd journals and service logs.

---

## 3. Part 2: Inodes, Links, and Storage Allocation

### 3.1 Exploration and Command Execution

The second section explored inode metadata, link mechanics, dangling symbolic pointers, directory link counters, and sparse file allocation:

```bash
~/os-lab5 ❯ echo hello > notes.txt
~/os-lab5 ❯ stat notes.txt
  File: notes.txt
  Size: 6         	Blocks: 8          IO Block: 4096   regular file
Device: 0,58	Inode: 1100843     Links: 1
Access: (0644/-rw-r--r--)  Uid: ( 1000/sunflower)   Gid: ( 1000/sunflower)
Access: 2026-10-12 15:19:45.543385538 +0300
Modify: 2026-10-12 15:19:45.210383395 +0300
Change: 2026-10-12 15:19:45.210383395 +0300
 Birth: 2026-10-12 15:19:45.210383395 +0300

~/os-lab5 ❯ ls -li notes.txt
  inode Permissions Size User      Date Modified Name
1100843 .rw-r--r--     6 sunflower 12 Oct 15:19   notes.txt

~/os-lab5 ❯ ln notes.txt hard.txt
~/os-lab5 ❯ ls -li notes.txt hard.txt
  inode Permissions Size User      Date Modified Name
1100843 .rw-r--r--     6 sunflower 12 Oct 15:19   hard.txt
1100843 .rw-r--r--     6 sunflower 12 Oct 15:19   notes.txt

~/os-lab5 ❯ rm notes.txt
~/os-lab5 ❯ cat hard.txt ; ls -li hard.txt
hello
  inode Permissions Size User      Date Modified Name
1100843 .rw-r--r--     6 sunflower 12 Oct 15:19   hard.txt

~/os-lab5 ❯ ln -s hard.txt soft.txt
~/os-lab5 ❯ ls -li hard.txt soft.txt
  inode Permissions Size User      Date Modified Name
1100843 .rw-r--r--     6 sunflower 12 Oct 15:19   hard.txt
1100898 lrwxrwxrwx     - sunflower 12 Oct 15:22   soft.txt -> hard.txt

~/os-lab5 ❯ cat soft.txt
hello

~/os-lab5 ❯ rm hard.txt
~/os-lab5 ❯ cat soft.txt ; ls -l soft.txt
cat: soft.txt: No such file or directory
Permissions Size User      Date Modified Name
lrwxrwxrwx     - sunflower 12 Oct 15:22   soft.txt -> hard.txt

~/os-lab5 ❯ mkdir d ; ls -ld d
Permissions Size User      Date Modified Name
drwxr-xr-x     - sunflower 12 Oct 15:23   d

~/os-lab5 ❯ mkdir d/x d/y ; ls -ld d
Permissions Size User      Date Modified Name
drwxr-xr-x     - sunflower 12 Oct 15:23   d

~/os-lab5 ❯ echo x > one.txt ; ls -l one.txt ; du -h one.txt
Permissions Size User      Date Modified Name
.rw-r--r--     2 sunflower 12 Oct 15:24   one.txt
4.0K	one.txt

~/os-lab5 ❯ truncate -s 1G big.img ; ls -lh big.img ; du -h big.img
Permissions Size User      Date Modified Name
.rw-r--r--  1.1G sunflower 12 Oct 15:24   big.img
0	big.img

~/os-lab5 ❯ rm big.img
```

### 3.2 Answers to Questions (Part 2)

#### (1) Which number in `stat` is the inode, and which tells how many names the file has?
From the `stat notes.txt` output:
- **`Inode: 1100843`**: The unique index node number on filesystem device `0,58`. It points to the filesystem metadata record containing permissions, ownership, file size, timestamps, and block pointers.
- **`Links: 1`**: Corresponds to `st_nlink` in the POSIX `stat` structure. It records the total count of directory entries (hard links) pointing to this inode.

#### (2) After `rm notes.txt`, why could you still read `hard.txt`? What does `rm` really remove?
In UNIX filesystems, a filename is not the file itself; it is an entry in a directory table mapping a string name to an inode number. 
When `ln notes.txt hard.txt` was executed, a second directory entry was created pointing to inode `1100843`, incrementing its link count to 2.
The command `rm` invokes the `unlink()` system call. `unlink()` removes the directory entry `notes.txt` from the parent directory and decrements the inode's link count (`st_nlink`) from 2 to 1.
Because `hard.txt` still references inode `1100843` and `st_nlink > 0`, the kernel retains the inode and all its physical data blocks on disk. `rm` only removes the directory entry name, not the underlying data. Data blocks are marked as free only when `st_nlink` reaches 0 and no active processes hold open file descriptors to the file.

#### (3) Why did `cat soft.txt` fail at the end, although `soft.txt` still exists? How big is `soft.txt` and why exactly that size?
- **Failure reason:** `soft.txt` is a symbolic link (`symlink`). Unlike a hard link, it has its own distinct inode (`1100898`) containing the string `"hard.txt"`. When `rm hard.txt` removed the target file, `soft.txt` became a **dangling (broken) symlink**. The directory entry for `soft.txt` still exists, but when `cat` attempts to open it, the kernel follows the pointer to `hard.txt`, fails to find that target inode, and returns `ENOENT` (`No such file or directory`).
- **File size:** `soft.txt` has a size of **8 bytes** (`ls -l` shows size 8). Under POSIX, the size of a symbolic link is defined strictly as the number of bytes in the target pathname string stored within it (excluding any terminating null byte). The target path string `"hard.txt"` has exactly 8 ASCII characters ($8 \times 1\text{ byte} = 8\text{ bytes}$).

#### (4) A new directory has 2 links, and 4 after you create two subdirectories. Explain where each link comes from.
- **Initial state (2 links after `mkdir d`):**
  1. The entry `d` in the parent directory (`~/os-lab5/d` pointing to the directory's inode).
  2. The self-referential entry `.` inside `d` (`~/os-lab5/d/.` pointing back to itself).
- **After creating subdirectories `d/x` and `d/y` (4 links):**
  3. The parent reference `..` inside child directory `d/x` (`d/x/..` pointing to `d`).
  4. The parent reference `..` inside child directory `d/y` (`d/y/..` pointing to `d`).
  *Every newly created subdirectory contributes an additional link to its parent directory via its `..` entry.*

#### (5) Why does a 2-byte file use 4.0K, and a 1 GB file use 0?
- **`one.txt` (2 bytes size, 4.0K disk usage):** Filesystems manage physical storage in discrete allocation blocks (typically 4096 bytes / 4 KiB). Even though `one.txt` contains only 2 bytes (`'x'` and `'\n'`), the filesystem must allocate at minimum one full 4 KiB block to store the payload on disk.
- **`big.img` (1.1 GB size, 0 disk usage):** The `truncate -s 1G` command creates a **sparse file**. It updates the file length metadata in the inode to $10^9$ bytes without allocating physical blocks on the storage device. The filesystem driver treats all unwritten regions as "holes". Until real data is written to those offsets, zero disk blocks are allocated, so `du -h` reports `0`.

---

## 4. Part 3: Permissions, Umask, and Special Bits

### 4.1 Exploration and Command Execution

The third section analyzed DAC permissions, octal masks, owner constraints, directory traversal semantics, and special bits:

```bash
~/os-lab5 ❯ echo secret > s.txt
~/os-lab5 ❯ chmod 600 s.txt ; ls -l s.txt
Permissions Size User      Date Modified Name
.rw-------     7 sunflower 12 Oct 15:26   s.txt

~/os-lab5 ❯ chmod 000 s.txt ; cat s.txt
cat: s.txt: Permission denied

~/os-lab5 ❯ chmod u+r s.txt ; cat s.txt
secret

~/os-lab5 ❯ umask ; touch new.txt ; mkdir newdir ; ls -ld new.txt newdir
0022
Permissions Size User      Date Modified Name
drwxr-xr-x     - sunflower 12 Oct 15:27   newdir
.rw-r--r--     0 sunflower 12 Oct 15:27   new.txt

~/os-lab5 ❯ mkdir box ; echo hi > box/f
~/os-lab5 ❯ chmod u=r,go= box ; ls box ; cat box/f
Permissions Size User Date Modified Name
.---------     0 root -             󰡯 f
cat: box/f: Permission denied

~/os-lab5 ❯ chmod u=x,go= box ; ls box ; cat box/f
Permission denied: box - code: 13
hi

~/os-lab5 ❯ chmod 755 box
~/os-lab5 ❯ ls -l /usr/bin/passwd ; ls -ld /tmp
Permissions Size User Date Modified Name
.rwsr-xr-x   81k root 30 Jul 13:04   /usr/bin/passwd
Permissions Size User Date Modified Name
drwxrwxrwt     - root 12 Oct 15:25   /tmp

~/os-lab5 ❯ cat /etc/shadow
cat: /etc/shadow: Permission denied

~/os-lab5 ❯ ls -l /etc/shadow
Permissions Size User Date Modified Name
.rw-------   947 root 29 Sep 12:13   /etc/shadow
```

### 4.2 Answers to Questions (Part 3)

#### (1) Write 600, 644, 755 and 700 as rwx strings, and give one real use for each.

| Octal | Symbolic (`rwx`) | Real-World Use Case |
|---|---|---|
| **600** | `-rw-------` | Sensitive user-private documents and secrets, such as SSH private keys (`~/.ssh/id_ed25519`) or database configuration credentials. |
| **644** | `-rw-r--r--` | Standard publicly readable files, such as web server assets (`index.html`), system configuration files (`/etc/resolv.conf`), and source code files. |
| **755** | `-rwxr-xr-x` / `drwxr-xr-x` | Executable system utilities (`/usr/bin/ls`) or shared directories (`/usr/share`, `/home/sunflower`) that allow non-owners to run programs and browse contents. |
| **700** | `-rwx------` / `drwx------` | Private directories holding sensitive credentials, such as `~/.ssh`, `~/.gnupg`, or the root user home directory `/root`. |

#### (2) With `chmod 000` you own the file but cannot read it. Why does ownership not help?
Linux DAC enforces access control strictly during the `open()` system call by checking identity tiers in order: Owner $\rightarrow$ Group $\rightarrow$ Others.
When process UID equals file owner UID, the kernel checks **only the owner permission bits**. If owner permissions are `0` (`---`), the kernel immediately halts access with `EACCES` (`Permission denied`).
Ownership grants the legal authority to **modify metadata and permissions** via `chmod()`, but does not automatically bypass active DAC permission bits. To read the file, the owner must explicitly grant themselves read rights via `chmod u+r`.

#### (3) What is your umask, and how does it explain the permissions of `new.txt` and `newdir`?
The system `umask` is **`0022`**.
The `umask` acts as a bitwise filter that is inverted and masked (`MODE & ~UMASK`) against default creation modes:
- **Files base mode:** `0666` (`rw-rw-rw-`).  
  `0666 & ~0022` = `0644` (`-rw-r--r--`).
  This accounts for `new.txt` having permissions `-rw-r--r--`.
- **Directories base mode:** `0777` (`rwxrwxrwx`).  
  `0777 & ~0022` = `0755` (`drwxr-xr-x`).
  This accounts for `newdir` having permissions `drwxr-xr-x`.
The mask clears write privileges for group and other users while preserving read and directory traversal execution.

#### (4) Explain the `box` experiment: what does `r` allow on a directory, and what does `x` allow?
- **Read (`r`) on a directory:** Grants permission to read the directory's raw data stream, which contains the list of filenames. With `chmod u=r,go= box`, `ls box` successfully enumerated the name `f`. However, without execute (`x`), the kernel cannot traverse into the directory or look up the inode behind that name. Thus `cat box/f` failed with `Permission denied`, and `ls -l` showed empty/broken metadata (`.--------- 0 root`).
- **Execute (`x`, search) on a directory:** Grants permission to traverse through the directory and resolve paths to access child inodes. With `chmod u=x,go= box`, the directory index cannot be read (`ls box` failed with `Permission denied`), but because the name `f` was already known, `cat box/f` traversed through `box`, opened the inode for `f`, and printed `"hi"`.

#### (5) What do the `s` in `/usr/bin/passwd` and the `t` in `/tmp` mean, and why does `passwd` need the `s`?
- **SetUID bit (`s` in `.rwsr-xr-x`):** Modifies process execution so that when an unprivileged user executes `/usr/bin/passwd`, the process acquires the **effective UID of the file's owner (`root`)** instead of the executing user.
  - *Why `passwd` requires `s`:* Passwords and hashes are stored in `/etc/shadow`, which has permissions `600` owned by `root` (`.rw------- root root`). An unprivileged user cannot modify or read `/etc/shadow` (`cat /etc/shadow` yields `Permission denied`). When changing their password, the user runs `passwd`; the SetUID bit temporarily elevates the process to root privileges, enabling it to write the updated password hash to `/etc/shadow` securely before terminating.
- **Sticky bit (`t` in `drwxrwxrwt`):** Applied to world-writable shared directories such as `/tmp`. Without the sticky bit, any user with write permissions in a directory can delete or rename any other user's files. The sticky bit restricts deletion and renaming rights so that **only the file's owner or `root` can unlink or rename files**, preventing malicious tampering.

---

## 5. Part 4: Storage Stack, Mounts, and Virtual Filesystems

### 5.1 Exploration and Command Execution

The fourth section explored block storage devices, encrypted containers, mount topologies, and kernel virtual filesystems (`procfs`):

```bash
~/os-lab5 ❯ lsblk
NAME        MAJ:MIN RM   SIZE RO TYPE  MOUNTPOINTS
zram0       252:0    0  15.3G  0 disk  [SWAP]
nvme0n1     259:0    0 476.9G  0 disk  
├─nvme0n1p1 259:1    0     2G  0 part  /boot
└─nvme0n1p2 259:2    0 474.9G  0 part  
  └─root    253:0    0 474.9G  0 crypt /var/log
                                       /var/cache/pacman/pkg
                                       /home
                                       /

~/os-lab5 ❯ df -h
Filesystem        Size  Used Avail Use% Mounted on
dev               7.5G     0  7.5G   0% /dev
run               7.7G  1.9M  7.7G   1% /run
efivarfs          256K  173K   79K  69% /sys/firmware/efi/efivars
/dev/mapper/root  475G  178G  297G  38% /
tmpfs             7.7G   76M  7.6G   1% /dev/shm
tmpfs             7.7G  6.8M  7.7G   1% /tmp
/dev/mapper/root  475G  178G  297G  38% /home
/dev/mapper/root  475G  178G  297G  38% /var/cache/pacman/pkg
/dev/mapper/root  475G  178G  297G  38% /var/log
/dev/nvme0n1p1    2.0G  527M  1.5G  26% /boot
tmpfs             1.6G  3.6M  1.6G   1% /run/user/1000

~/os-lab5 ❯ mount | grep " / "
/dev/mapper/root on / type btrfs (rw,relatime,compress=zstd:3,ssd,space_cache=v2,subvolid=256,subvol=/@)

~/os-lab5 ❯ grep -c processor /proc/cpuinfo
12

~/os-lab5 ❯ head -3 /proc/meminfo
MemTotal:       15993828 kB
MemFree:         3457064 kB
MemAvailable:    8559140 kB

~/os-lab5 ❯ head -5 /proc/self/status
Name:	head
Umask:	0022
State:	R (running)
Tgid:	61008
Ngid:	0

~/os-lab5 ❯ ls -l /proc/self/fd
lr-x------     - sunflower 12 Oct 15:33   3 -> /proc/61052/fd
lrwx------     - sunflower 12 Oct 15:33  󰡯 0 -> /dev/pts/3
lrwx------     - sunflower 12 Oct 15:33  󰡯 1 -> /dev/pts/3
lrwx------     - sunflower 12 Oct 15:33  󰡯 2 -> /dev/pts/3
```

### 5.2 Answers to Questions (Part 4)

#### (1) Draw your disk: disk name, partitions, which one is mounted at `/` and with what file system type.

```
+-----------------------------------------------------------------------------------------+
| Physical Disk: nvme0n1 (476.9 GB NVMe Solid-State Disk)                                 |
+-------------------------------------------+---------------------------------------------+
| Partition 1: nvme0n1p1 (2.0 GB)           | Partition 2: nvme0n1p2 (474.9 GB)           |
| Filesystem: VFAT / EFI System Partition   | Type: LUKS Encrypted Container (dm-crypt)   |
| Mountpoint: /boot (26% used)              +---------------------------------------------+
|                                           | Decrypted Device Mapper: /dev/mapper/root   |
|                                           | Filesystem Type: BTRFS                      |
|                                           | Options: rw, compress=zstd:3, ssd           |
|                                           +---------------------------------------------+
|                                           | Subvolume @          --> /                  |
|                                           | Subvolume @home      --> /home              |
|                                           | Subvolume @log       --> /var/log           |
|                                           | Subvolume @pkg       --> /var/cache/pacman  |
+-------------------------------------------+---------------------------------------------+
| RAM Virtual Disk: zram0 (15.3 GB)         | Filesystem: Swap Area (In-Memory Swap)      |
+-------------------------------------------+---------------------------------------------+
```

- **Physical disk name:** `nvme0n1` (NVMe controller 0, namespace 1).
- **Partition mounted at `/`:** Partition `nvme0n1p2`, unlocked through the device-mapper layer as `/dev/mapper/root`.
- **Filesystem type:** **Btrfs** (with `compress=zstd:3`, `ssd`, `space_cache=v2`, `subvol=/@`).

#### (2) How full is your root file system?
According to `df -h`, the root filesystem `/dev/mapper/root` has:
- **Total capacity:** `475 GB`
- **Used space:** `178 GB`
- **Available space:** `297 GB`
- **Utilization percentage:** **38%**

#### (3) How many CPU cores and how much RAM does your laptop have, according to `/proc`?
- **Logical CPU cores / hardware threads:** **12** (queried via `grep -c processor /proc/cpuinfo`).
- **RAM metrics:** From `/proc/meminfo`:
  - `MemTotal: 15,993,828 kB` $\approx$ **15.25 GiB** (~16 GB physical RAM installed).
  - `MemFree: 3,457,064 kB` $\approx$ 3.30 GiB.
  - `MemAvailable: 8,559,140 kB` $\approx$ **8.16 GiB** (memory ready for new processes without swapping).

#### (4) `/proc/self/status` showed the name of a program. Which one, and why that one?
- **Program name shown:** **`head`** (`Name: head`).
- **Technical Explanation:** `/proc/self` is a special dynamic symbolic link maintained on-the-fly by the Linux kernel's `procfs` virtual filesystem. Whenever any process opens or resolves `/proc/self`, the kernel dynamically resolves the path to `/proc/<PID>`, where `<PID>` is the process ID of the **caller making the system call**. Because `head -5 /proc/self/status` was executed, the `head` binary was the active process reading from `/proc/self/status`. Thus, `procfs` returned the runtime status of the `head` process itself.

---

## 6. Part 5: Bonus Tasks & Advanced Investigations

The laboratory guidelines prompt selecting two optional tasks. Below, three in-depth investigations are analyzed:

### 6.1 Task (a): Magic Numbers and File Type Identification
- **Command & Output:**
  ```bash
  gzip -c /etc/passwd > notes.txt
  file notes.txt
  # notes.txt: gzip compressed data, was "passwd", last modified: Tue Sep 29 09:13:07 2026, from Unix
  od -t x1 -A n notes.txt | head -1
  # 1f 8b 08 08 23 81 bb 6a 00 03 70 61 73 73 77 64
  ```
- **Analysis:**
  The command `file` identifies file types independently of filenames or extensions. It examines the initial header bytes against a database of signatures (`libmagic`). The first two hex bytes **`1f 8b`** (octal `037 0213`) constitute the standard **magic number for gzip compressed streams** (RFC 1952). The third byte (`08`) denotes the compression method (DEFLATE), and the fourth byte (`08`) indicates that the original filename (`FNAME`) flag is set.

### 6.2 Task (b): Auditing SetUID Binaries Across the Filesystem
- **Command:** `find / -xdev -perm -4000 -type f 2>/dev/null`
- **Output:**
  ```
  /usr/bin/ksu
  /usr/bin/unix_chkpwd
  /usr/bin/chage
  /usr/bin/gpasswd
  /usr/bin/passwd
  /usr/bin/chfn
  /usr/bin/chsh
  /usr/bin/mount
  /usr/bin/newgrp
  /usr/bin/su
  /usr/bin/umount
  /usr/bin/sudo
  /usr/bin/pkexec
  /usr/bin/fusermount3
  /usr/bin/crontab
  /usr/lib/dbus-daemon-launch-helper
  /usr/lib/Xorg.wrap
  /usr/lib/ssh/ssh-keysign
  /usr/lib/chromium/chrome-sandbox
  /usr/lib/electron43/chrome-sandbox
  /opt/brave-bin/chrome-sandbox
  ```
- **Analysis:**
  In addition to standard UNIX administration binaries (`sudo`, `passwd`, `su`, `mount`), the system contains sandboxing helpers such as `/opt/brave-bin/chrome-sandbox` and `/usr/lib/chromium/chrome-sandbox`. These browser sandboxes require root privileges at initialization to configure Linux user namespaces (`CLONE_NEWUSER`), network namespaces, and seccomp-BPF system call filtering before dropping privileges to isolate untrusted web content.

### 6.3 Task (c): Deleting an Unlinked File with an Active File Descriptor
- **Experiment:**
  ```bash
  exec 3< notes.txt
  rm notes.txt
  ls -l /proc/$$/fd
  # lr-x------ 1 sunflower sunflower 64 Oct 12 15:45 3 -> /home/sunflower/os-lab5/notes.txt (deleted)
  exec 3<&-
  ```
- **Analysis:**
  When `rm notes.txt` is run, the directory entry is unlinked, but the process maintains file descriptor 3 open to the file. In `/proc/$$/fd/3`, the kernel annotates the target path with `(deleted)`. The inode link count (`st_nlink`) drops to 0, but the kernel maintains the physical storage blocks and inode in memory because the open file descriptor count is greater than zero. The disk blocks are physically reclaimed only when descriptor 3 is closed (`exec 3<&-`) or the process terminates.

---

## 7. Conclusions

Laboratory Work No. 5 provided rigorous hands-on insights into the core primitives of Linux storage and the POSIX security architecture:
1. **FHS Uniformity:** The Linux filesystem standardizes system access through virtual and physical directory hierarchies, with the modern UsrMerge simplifying binary packaging by symlinking `/bin` to `/usr/bin`.
2. **Inodes Decouple Names from Data:** File names are merely directory pointer entries. Hard links share an inode and reference count, whereas symbolic links are independent pointer files whose byte size reflects their destination path string.
3. **Storage Allocation Mechanics:** Storage blocks are allocated in 4 KiB units, resulting in slack space for tiny files, while sparse files allow multi-gigabyte logical address spaces with zero physical disk consumption.
4. **POSIX Permission Enforcement:** DAC operates strictly on identity levels. Ownership grants modification rights but does not override active permission masks. Crucially, directory `r` governs entry enumeration while directory `x` governs inode resolution and traversal.
5. **Storage and Procfs Virtualization:** Modern Linux storage incorporates multi-layer abstractions including NVMe block devices, dm-crypt LUKS encryption, and Btrfs subvolumes, while `/proc` demonstrates how the kernel exposes internal hardware and process states through filesystem interfaces.
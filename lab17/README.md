# Lab 17: Threads Share Memory, and Race (Track A: Linux)

This repository directory contains the complete source code, benchmarks, and formal report for **Operating Systems -- Lab 17**.

## Contents

- `fire.c`: Part 1 implementation with two threads concurrently incrementing a shared counter $10^6$ times without synchronization, demonstrating lost updates due to data races.
- `fire4.c`: Part 2 implementation scaling to four concurrent threads, demonstrating increased collision rates and greater lost update magnitudes ($>70\%$ loss).
- `fire_safe.c`: Part 2 proposed fix using `pthread_mutex_t` to guarantee mutual exclusion and achieve exact counter convergence ($4 \times 10^6$).
- `Makefile`: Build rules compiling `fire`, `fire4`, and `fire_safe` with `-Wall -Wextra -pthread -O2`.
- `results.txt`: Terminal execution transcripts across multiple runs of `fire`, `fire4`, and `fire_safe`.
- `notes.txt`: Direct answers to the three Observe questions from the lab assignment.
- `report.md`: Comprehensive laboratory report in GitHub Markdown.
- `report.tex` / `report.pdf` / `Lab17_IulianPavlov_FAF-241.pdf`: Formatted IEEE/academic PDF report compiled via Tectonic.
- `logo.png`: UTM institutional logo for report title page.

## Building and Running

```bash
make
./fire
./fire4
./fire_safe
```


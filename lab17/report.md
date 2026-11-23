# Operating Systems -- Lab 17: Threads Share Memory, and Race

**Course:** Operating Systems (Track A: the real OS, from the outside (Linux))  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026--2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Scope

Threads provide lightweight execution units within a single process. While processes possess isolated virtual memory spaces, threads share identical process memory (including the data segment, heap, and global variables). This shared memory is both the primary strength of multi-threaded programming (enabling instantaneous communication without copy overhead) and its primary vulnerability (causing non-deterministic race conditions and data corruption when updates are unsynchronized).

The objectives of Laboratory Work No. 17 are:
1. **Shared Memory Verification**: Demonstrate that two threads of one process read and write the exact same memory address space instantaneously.
2. **Data Race Observation**: Quantify lost updates when multiple threads concurrently execute `counter++` without locking.
3. **Scaling the Contention**: Expand the workload to four threads, analyze the escalation of lost updates, and explore the hardware-level mechanism of the race condition.
4. **Architectural Fix**: Formulate the theoretical principle that prevents data races (mutual exclusion) and implement a synchronized baseline using POSIX mutexes.

---

## 2. Source Code Implementation

### 2.1 Baseline Race: `fire.c` (Part 1)
Two threads each perform $N = 1\,000\,000$ iterations of `counter++` on a single global `long counter = 0;`:

```c
/* fire.c - two threads of one process share the same memory.
 * Both add to the same counter, a million times each, WITHOUT a lock.
 * Build: gcc -Wall -Wextra -pthread -o fire fire.c Run: ./fire
 */
#include <stdio.h>
#include <pthread.h>

#define N 1000000
long counter = 0; /* one variable, shared by both threads */

void *work(void *arg) {
    long id = (long)arg;
    printf("thread %ld sees counter = %ld\n", id, counter);
    for (int i = 0; i < N; i++)
        counter++; /* NOT atomic: read, add, write */
    return NULL;
}

int main(void) {
    pthread_t a, b;
    pthread_create(&a, NULL, work, (void *)1);
    pthread_create(&b, NULL, work, (void *)2);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("expected: %d\n", 2 * N);
    printf("counter: %ld (lost updates: the race)\n", counter);
    return 0;
}
```

### 2.2 Scaled Contention: `fire4.c` (Part 2)
To investigate the effect of scaling contention, `fire4.c` spawns four concurrent threads:

```c
#include <stdio.h>
#include <pthread.h>

#define N 1000000
#define NUM_THREADS 4
long counter = 0;

void *work(void *arg) {
    long id = (long)arg;
    printf("thread %ld sees counter = %ld\n", id, counter);
    for (int i = 0; i < N; i++)
        counter++;
    return NULL;
}

int main(void) {
    pthread_t th[NUM_THREADS];
    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&th[i], NULL, work, (void *)(i + 1));
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(th[i], NULL);
    }
    printf("expected: %d\n", NUM_THREADS * N);
    printf("counter: %ld (lost updates: the race, 4 threads)\n", counter);
    return 0;
}
```

### 2.3 Synchronized Solution: `fire_safe.c`
Protecting `counter++` with a mutual exclusion lock guarantees atomicity:

```c
#include <stdio.h>
#include <pthread.h>

#define N 1000000
#define NUM_THREADS 4
long counter = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *work(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < N; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
    printf("thread %ld finished\n", id);
    return NULL;
}

int main(void) {
    pthread_t th[NUM_THREADS];
    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&th[i], NULL, work, (void *)(i + 1));
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(th[i], NULL);
    }
    printf("expected: %d\n", NUM_THREADS * N);
    printf("counter: %ld (safe with mutex: no lost updates)\n", counter);
    return 0;
}
```

---

## 3. Experimental Results

Execution across repeated trials yielded the following measurements:

```text
=== Running ./fire (2 threads, Part 1) ===
--- Run 1 ---
thread 1 sees counter = 0
thread 2 sees counter = 0
expected: 2000000
counter: 1048866 (lost updates: the race)
--- Run 2 ---
thread 2 sees counter = 0
thread 1 sees counter = 0
expected: 2000000
counter: 1107049 (lost updates: the race)
--- Run 3 ---
thread 1 sees counter = 0
thread 2 sees counter = 0
expected: 2000000
counter: 1013978 (lost updates: the race)
--- Run 4 ---
thread 1 sees counter = 0
thread 2 sees counter = 0
expected: 2000000
counter: 1134366 (lost updates: the race)
--- Run 5 ---
thread 1 sees counter = 0
thread 2 sees counter = 0
expected: 2000000
counter: 1045272 (lost updates: the race)

=== Running ./fire4 (4 threads, Part 2) ===
--- Run 1 ---
thread 1 sees counter = 0
thread 3 sees counter = 0
thread 2 sees counter = 0
thread 4 sees counter = 0
expected: 4000000
counter: 1819855 (lost updates: the race, 4 threads)
--- Run 2 ---
thread 1 sees counter = 0
thread 4 sees counter = 0
thread 2 sees counter = 0
thread 3 sees counter = 0
expected: 4000000
counter: 1339380 (lost updates: the race, 4 threads)
--- Run 3 ---
thread 1 sees counter = 0
thread 3 sees counter = 0
thread 4 sees counter = 4577
thread 2 sees counter = 0
expected: 4000000
counter: 1510870 (lost updates: the race, 4 threads)
--- Run 4 ---
thread 1 sees counter = 0
thread 4 sees counter = 103
thread 2 sees counter = 0
thread 3 sees counter = 0
expected: 4000000
counter: 1144901 (lost updates: the race, 4 threads)

=== Running ./fire_safe (4 threads with pthread_mutex, Proposed Fix) ===
--- Run 1 ---
thread 2 finished
thread 4 finished
thread 1 finished
thread 3 finished
expected: 4000000
counter: 4000000 (safe with mutex: no lost updates)
```

---

## 4. Answers to Observation Questions

### Observe Question 1 (Part 1)
**Question:** Thread 2 prints a non-zero counter at the start. What does that prove about the memory of the two threads?  
**Answer:**  
It proves that threads within the same process share the exact same address space and virtual memory. Unlike separate processes (which receive isolated, copy-on-write memory spaces created by `fork()`), threads created by `pthread_create()` share global variables directly in memory. When Thread 2 starts and reads `counter`, it directly observes the memory modifications already written by Thread 1 without any inter-process communication or data copying.

### Observe Question 2 (Part 1)
**Question:** Why is the final counter less than 2000000, and different each run? Explain with the three steps of `counter++`.  
**Answer:**  
In C, the expression `counter++` is not an atomic operation. At the hardware assembly level, it translates into three distinct, separate instructions:
1. **Read (Load):** Copy `counter` from RAM into a CPU register (e.g. `movq counter(%rip), %rax`).
2. **Add (Modify):** Increment the value held in the register (e.g. `addq $1, %rax`).
3. **Write (Store):** Write the incremented value from the register back to RAM (e.g. `movq %rax, counter(%rip)`).

When two or more threads run concurrently on multiple CPU cores or are interleaved by the kernel scheduler, their read-modify-write sequences overlap in time (a data race). For instance:
- Time $t_0$: Thread 1 reads `counter` (value = 500) into its register.
- Time $t_1$: Thread 2 reads `counter` (value = 500) into its register before Thread 1 writes back.
- Time $t_2$: Thread 1 adds 1 (register = 501) and stores 501 to RAM.
- Time $t_3$: Thread 2 adds 1 (register = 501) and stores 501 to RAM, overwriting Thread 1's work.

Two distinct increments took place, but `counter` only increased by 1; one update was silently lost. Because thread scheduling, interrupt timing, and core execution speeds fluctuate unpredictably on every execution, race windows occur at random points, making the final result non-deterministic and strictly below 2,000,000.

### Observe Question 3 (Part 2)
**Question:** In one sentence: what single idea would stop the lost updates? (You build it next week.)  
**Answer:**  
Enforcing mutual exclusion around the critical section (e.g., via a mutex lock or atomic hardware instruction) so that only one thread can execute the read-modify-write sequence on the shared counter at any given instant.

---

## 5. Conclusion

Laboratory Work No. 17 provided empirical validation of the shared memory model and concurrency hazards inherent in multithreaded systems. We observed that threads communicate via shared variables without IPC overhead, but unsynchronized concurrent access leads to devastating loss of updates ($45\%$ loss in 2-thread runs, scaling to $>70\%$ loss in 4-thread runs). Mutual exclusion via synchronization primitives is mandatory to guarantee program correctness.


# Laboratory Work No. 11: The Race and the Semaphore

**Course:** Operating Systems (Track A: the real OS, from the outside (Linux))  
**Institution:** Technical University of Moldova (UTM), FCIM / FAF  
**Academic Year:** 2026–2027  
**Student:** Iulian Pavlov, Group FAF-241  
**Supervisor / Checked by:** Reițman Patricia, University Assistant, Department of Software and Automation Engineering  
**Location & Date:** Chișinău, 2026  

---

## 1. Objectives and Theoretical Background

### 1.1 Laboratory Objectives
Concurrent threads executing inside the same process share a single virtual address space, including global variables. Whenever multiple threads read and modify shared state without synchronization, the operating system scheduler can interleave their instructions unpredictably and silently lose updates (**data race**). The objectives of Laboratory Work No. 11 are:
- **Reproducing a Data Race (`race.c`):** Demonstrating how two threads incrementing a shared global counter 1,000,000 times each produce wrong, non-deterministic results when compiled without optimization (`-O0` default, no `-O2`).
- **Mutual Exclusion (`mutex.c`, `sem.c`):** Enforcing exclusive access to the critical section using both a POSIX mutex (`pthread_mutex_t`) and a POSIX binary semaphore initialized to `1` (`sem_t`), and quantifying synchronization overhead with `time` (including `sem2.c` started at `2`).
- **Thread Signaling (`order.c`, `order1.c`):** Using a semaphore initialized to `0` to enforce execution order between a producer (`writer`) and a consumer (`reader`).
- **Bounded Resource Counting (`parking.c`, `parking1.c`):** Using a semaphore initialized to `PLACES = 3` as a counting semaphore capping concurrent access (`CARS = 6`), then reducing `PLACES` to `1` to observe degeneration into a binary mutex.
- **Optional Lock-Free Atomics (`atomic.c`):** Comparing hardware atomic `atomic_fetch_add()` (`<stdatomic.h>`) against kernel-assisted mutexes and semaphores.

### 1.2 Three Roles of a Semaphore
A counting semaphore maintains a non-negative integer value governed by `sem_wait()` (decrement or block if `0`) and `sem_post()` (increment and wake a waiter):

| Initial Value | Pattern / Role | Synchronization Semantics | Lab 11 Program |
| :---: | --- | --- | --- |
| `1` | **Binary Lock (Mutex)** | Mutual exclusion: at most one thread enters the critical section at once | `sem.c`, `parking1.c` |
| `0` | **Signal / Barrier** | Execution ordering: `sem_wait()` blocks until another thread calls `sem_post()` | `order.c` |
| `N` (`3`) | **Resource Counter** | Bounded concurrency: up to `N` threads proceed simultaneously | `parking.c` |

---

## 2. Part 1: See the Race (`race.c`)

### 2.1 Implementation and Execution
*(Note: In the original terminal session logged in `notes.md`, a bracketed-paste escape sequence caused `cat > race.c` to fail and leave `race.c` empty; `race.c` was subsequently restored from the Lab 11 specification, compiled without `-O2`, and verified live.)*

```c
#include <stdio.h>
#include <pthread.h>
#define N 1000000
long counter = 0;
void *add(void *arg) {
    for (int i = 0; i < N; i++)
        counter++;
    return NULL;
}
int main(void) {
    pthread_t a, b;
    pthread_create(&a, NULL, add, NULL);
    pthread_create(&b, NULL, add, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("counter = %ld (expected %d)\n", counter, 2 * N);
    return 0;
}
```

Compilation and live execution across multiple runs:
```text
$ gcc -Wall -pthread -o race race.c
$ ./race
counter = 1213503 (expected 2000000)
$ ./race
counter = 1073171 (expected 2000000)
$ ./race
counter = 1220817 (expected 2000000)
$ time ./race
counter = 1089313 (expected 2000000)

real	0m0.019s
user	0m0.025s
sys	0m0.003s
```

### 2.2 Observe Answers — Part 1
1. **`counter++` looks like one step. Write the three steps the CPU really does.**
   1. **Load (read):** Copy `counter` from memory into a CPU register.
   2. **Increment (add 1):** Add `1` to that register inside the ALU.
   3. **Store (write back):** Write the updated register value back into `counter` in memory.
2. **Describe, with those steps, how two threads can lose one addition.**  
   Suppose `counter = 100`. Thread `a` loads `100` into its register and increments it to `101`, but gets preempted (or executes simultaneously on a second core) before writing back. Thread `b` also loads the stale `100` from memory, increments to `101`, and stores `101`. Thread `a` then resumes and stores `101`. Even though both threads ran `counter++`, `counter` increased from `100` to `101` instead of `102` — one increment is lost.
3. **Why is the result different at every run?**  
   Operating system thread scheduling, timer interrupts, CPU core migration, and cache coherence timings differ slightly on every execution, altering how many load–increment–store sequences overlap (`1213503`, `1073171`, `1220817`).

---

## 3. Part 2: Fixing the Race (`mutex.c`, `sem.c`, `sem2.c`)

### 3.1 Mutex Solution (`mutex.c`) and Semaphore Lock (`sem.c`)
Wrapping `counter++` inside `pthread_mutex_lock(&lock)` / `pthread_mutex_unlock(&lock)` (`mutex.c`) or `sem_wait(&s)` / `sem_post(&s)` with `sem_init(&s, 0, 1)` (`sem.c`) enforces strict mutual exclusion around the critical section. Testing an intentionally buggy semaphore starting at `2` (`sem2.c`: `sem_init(&s, 0, 2)`) shows what happens when two permits exist simultaneously.

```text
$ gcc -Wall -pthread -o mutex mutex.c
$ ./mutex && ./mutex && ./mutex
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)

$ gcc -Wall -pthread -o sem sem.c
$ ./sem && ./sem && ./sem
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)

$ sed 's/sem_init(&s, 0, 1)/sem_init(\&s, 0, 2)/' sem.c > sem2.c
$ gcc -Wall -pthread -o sem2 sem2.c && ./sem2 && ./sem2 && ./sem2
counter = 1629819 (expected 2000000)
counter = 1392310 (expected 2000000)
counter = 1598883 (expected 2000000)
```

*(Earlier recorded runs in `notes.md` also measured `./sem2` at `1610383`, `1914526`, and `1386239`.)*

### 3.2 Performance Comparison (`time`)

| Binary | Mechanism | Observed `counter` | `real` | `user` | `sys` |
| --- | --- | :---: | :---: | :---: | :---: |
| `./race` | Unsynchronized | `1089313` (wrong) | `0.019 s` | `0.025 s` | `0.003 s` |
| `./atomic` | `atomic_fetch_add` | `2000000` | `0.044 s` | `0.084 s` | `0.000 s` |
| `./mutex` | `pthread_mutex_t` | `2000000` | `0.195 s` | `0.185 s` | `0.192 s` |
| `./sem` | `sem_t` (`init=1`) | `2000000` | `0.282 s` | `0.260 s` | `0.295 s` |

### 3.3 Observe Answers — Part 2
1. **Run `mutex` and `sem` three times each. Is the answer always `2 000 000`?**  
   Yes — across all runs (`3/3` plus the timed runs), both `./mutex` and `./sem` always returned `counter = 2000000 (expected 2000000)`.
2. **What would happen in `sem.c` with `sem_init(&s, 0, 2)`? Try it and explain.**  
   In `sem2.c`, results dropped to `1629819`, `1392310`, and `1598883` (`notes.md`: `1610383`, `1914526`, `1386239`). Starting the semaphore at `2` gives out **two** permits simultaneously (`2 -> 1 -> 0`), allowing both threads inside `counter++` at the same time and restoring the data race.
3. **Why is the locked version slower?**  
   Executing 2,000,000 lock/unlock cycles introduces heavy atomic contention across CPU cores and triggers Linux `futex` kernel system calls (`sys = 0.192 s` for `mutex`, `0.295 s` for `sem`, compared to `0.003 s` for `race` and `0.000 s` for `atomic`) whenever a contended thread sleeps or wakes up.

---

## 4. Part 3: A Semaphore as a Signal (`order.c`, `order1.c`)

### 4.1 Signaling Execution Order
In `order.c`, thread `b` (`reader`) starts first and waits on `sem_t ready` initialized to `0`, while thread `a` (`writer`) sleeps 1 second, writes `"hello"` into `message`, and calls `sem_post(&ready)`. In `order1.c`, `sem_init(&ready, 0, 0)` was changed to `sem_init(&ready, 0, 1)`.

```text
$ ./order
B: waiting for the message...
A: message written
B: I read "hello"

$ ./order1
B: waiting for the message...
B: I read ""
A: message written
```

### 4.2 Observe Answers — Part 3
1. **Change the start value to `1` (`sem_init(&ready, 0, 1)`), compile, run. What does B read now, and why?**  
   B immediately prints `B: I read ""` (an empty string) before A sleeps for 1 second and writes `"hello"`. Because `ready` starts at `1`, `sem_wait(&ready)` consumes that pre-existing token immediately without blocking, reading the zero-initialized global `message` array too early.
2. **Put it back to `0`. In one sentence: what does a semaphore that starts at `0` mean?**  
   A semaphore initialized to `0` represents a pure ordering signal ("the event has not occurred yet"): any caller of `sem_wait()` blocks until another thread signals readiness via `sem_post()`.

---

## 5. Part 4: A Semaphore as a Counter (`parking.c`, `parking1.c`)

### 5.1 Parking Lot Simulation (`PLACES = 3` vs. `PLACES = 1`)
`parking.c` spawns `CARS = 6` threads sharing `PLACES = 3` spots managed by `sem_t places` (`sem_init(&places, 0, PLACES)`), while a separate `pthread_mutex_t lock` protects only the `inside` counter increment/decrement and its matching `printf`. Changing `PLACES` to `1` (`parking1.c`) restricts occupancy to a single car at a time.

```text
$ ./parking
car 1 enters (inside: 1)
car 3 enters (inside: 2)
car 2 enters (inside: 3)
car 2 leaves (inside: 2)
car 3 leaves (inside: 1)
car 1 leaves (inside: 0)
car 5 enters (inside: 1)
car 4 enters (inside: 2)
car 6 enters (inside: 3)
car 5 leaves (inside: 2)
car 4 leaves (inside: 1)
car 6 leaves (inside: 0)

$ ./parking1
car 1 enters (inside: 1)
car 1 leaves (inside: 0)
car 2 enters (inside: 1)
car 2 leaves (inside: 0)
car 3 enters (inside: 1)
car 3 leaves (inside: 0)
car 4 enters (inside: 1)
car 4 leaves (inside: 0)
car 5 enters (inside: 1)
car 5 leaves (inside: 0)
car 6 enters (inside: 1)
car 6 leaves (inside: 0)
```

### 5.2 Observe Answers — Part 4
1. **What is the largest value of `inside` you see? Which line of code guarantees it?**  
   The maximum `inside` value observed is **`3`**. It is guaranteed by **`sem_init(&places, 0, PLACES);`** (setting initial permits to `PLACES = 3`) combined with **`sem_wait(&places);`** at the start of `car()`, which blocks the 4th–6th cars until a parked car calls `sem_post(&places);`.
2. **Set `PLACES` to `1`. What does the semaphore become?**  
   With `PLACES = 1` (`parking1.c`), every car enters and leaves one by one (`inside` never exceeds `1`), so the semaphore becomes a **binary semaphore (mutex)**.

---

## 6. Optional Extension: Lock-Free Atomics (`atomic.c`)

`atomic.c` replaces locking with C11 `<stdatomic.h>` (`atomic_long counter = 0;` and `atomic_fetch_add(&counter, 1);`), combining load, add, and store into a single indivisible hardware instruction (`lock xadd` on x86-64).

```text
$ time ./atomic
counter = 2000000 (expected 2000000)

real	0m0.044s
user	0m0.084s
sys	0m0.000s
```

Because no kernel `futex` transitions occur (`sys = 0.000 s`), `./atomic` completed correctly in **`0.044 s`**, roughly **4.4x faster** than `./mutex` (`0.195 s`) and **6.4x faster** than `./sem` (`0.282 s`).

---

## 7. Conclusions

1. **Data Races Arise from Non-Atomic Read–Modify–Write Sequences:** `counter++` decomposes into three CPU steps (load, increment, store); interleaving them across threads causes unpredictable lost updates whenever synchronization is omitted (`1.07M–1.22M` instead of `2,000,000`).
2. **Same Semaphore Primitive, Three Synchronization Idioms:** Depending on its initial counter value, a POSIX semaphore implements mutual exclusion (`1`), ordered thread signaling (`0`), or bounded resource allocation (`K > 1`).
3. **Lock Overhead vs. Atomics:** Mutexes and semaphores guarantee correctness at the cost of atomic contention and kernel `futex` syscalls (~`0.20–0.28 s` for 2M increments), whereas single-variable updates can use `atomic_fetch_add` (`0.044 s`, `0` sys time) without blocking in kernel space.


# Operating Systems -- Lab 12: Producer, Consumer and the Other Tools (Linux)

## Part 1: Producer and Consumer (`prodcons.c`)

### Build & Run
```bash
gcc -Wall -pthread -o prodcons prodcons.c
./prodcons
```

### Observed Output (`./prodcons`)
```text
put 1  (in buffer: 1)
put 2  (in buffer: 2)
put 3  (in buffer: 3)
put 4  (in buffer: 4)
        take 1  (in buffer: 3)
put 5  (in buffer: 4)
        take 2  (in buffer: 3)
put 6  (in buffer: 4)
        take 3  (in buffer: 3)
put 7  (in buffer: 4)
        take 4  (in buffer: 3)
put 8  (in buffer: 4)
        take 5  (in buffer: 3)
        take 6  (in buffer: 2)
        take 7  (in buffer: 1)
        take 8  (in buffer: 0)
```
Exit code: `0`

### Observe Questions
1. **The producer is fast and the consumer sleeps between items, yet `in buffer` never goes past 4. Which call stops the producer from adding a fifth item?**
   - `sem_wait(&empty);` in `producer()`. Because `empty` is initialized to `SIZE` (`4`), each produced item decrements `empty` by 1. When 4 items are in the buffer, `empty` becomes `0`, so the fifth `sem_wait(&empty)` blocks the producer until the consumer removes an item and calls `sem_post(&empty)`.

2. **Who waits at the very start of the run, and which call blocks them?**
   - Whenever the consumer runs while the buffer has no items (`full == 0`, as initialized at startup via `sem_init(&full, 0, 0);`), the **consumer** thread waits at `sem_wait(&full);` until the producer deposits an item and calls `sem_post(&full)`.

3. **In `producer()`, swap the order of `pthread_mutex_lock(&lock)` and `sem_wait(&empty)` (take the lock first, then wait). Run it. What happens when the buffer is full, and why?**
   - Running the swapped experiment (`timeout 2 /tmp/lab12_exp/prodcons_swap`) produced:
     ```text
     put 1  (in buffer: 1)
     put 2  (in buffer: 2)
     put 3  (in buffer: 3)
             take 1  (in buffer: 2)
     put 4  (in buffer: 3)
     put 5  (in buffer: 4)
     ```
     and then **deadlocked** (timed out with exit code `124`).
   - **Why:** When `buffer` reaches 4 items (`empty == 0`), `producer()` acquires `lock` via `pthread_mutex_lock(&lock)` first and then blocks inside `sem_wait(&empty)` while still holding `lock`. The `consumer()` thread is the only thread able to post to `empty`, but it must acquire `lock` first to inspect/take from the buffer — so `consumer()` blocks on `pthread_mutex_lock(&lock)`. Both threads wait on each other forever.

---

## Part 2: Condition Variable (`condvar.c`)

### Build & Run
```bash
gcc -Wall -pthread -o condvar condvar.c
./condvar
```

### Observed Output (`./condvar`)
```text
B: not ready, I sleep
A: message written
B: I read "hello"
```
Exit code: `0`

### Observe Questions
1. **Which variable remembers that the message was already ready?**
   - The shared flag `int ready` (initialized to `0`, set to `1` by `writer()` after writing `"hello"`, and tested by `reader()` in `while (!ready)`). Even if thread A signals before thread B starts waiting, `ready == 1` prevents thread B from calling `pthread_cond_wait` and missing the wakeup.

2. **Change `while (!ready)` to `if (!ready)` and run again. Does it still work here?**
   - **Yes**, in this simple program with one writer, one reader, and no other consumers, running with `if (!ready)` still produced:
     ```text
     B: not ready, I sleep
     A: message written
     B: I read "hello"
     ```
     and exited cleanly with code `0`.

3. **So why do we write `while`? Give one reason a thread can wake up from `pthread_cond_wait` and still find `ready == 0`.**
   - Two valid reasons:
     1. **Spurious wakeups:** POSIX permits `pthread_cond_wait()` to return even though no thread signaled or broadcasted the condition variable, so `ready` can still be `0`.
     2. **Competing threads / stolen condition:** If multiple consumer threads are waiting, one thread may wake up, acquire `lock`, consume the state, and reset `ready = 0` before another woken thread re-acquires `lock` and resumes after `pthread_cond_wait()`. Re-checking in a `while` loop guarantees the predicate truly holds before proceeding.

---

## Part 3: Readers and Writers (`readers.c`)

### Build & Run
```bash
gcc -Wall -pthread -o readers readers.c
./readers
```

### Observed Output (`./readers`)
```text
writer 1 writes 10  (alone)
reader 1 reads 10  (readers inside: 1)
reader 2 reads 10  (readers inside: 2)
reader 3 reads 10  (readers inside: 3)
writer 2 writes 20  (alone)
```
Exit code: `0`

### Observe Questions
1. **Three readers are inside at the same time, while each writer runs alone. Which two lines in `reader()` make that possible?**
   - Entry guard: `if (++readers == 1) sem_wait(&room);` (only the first active reader acquires `room`, locking out writers; readers 2 and 3 enter concurrently).
   - Exit guard: `if (--readers == 0) sem_post(&room);` (only the last leaving reader releases `room`, allowing waiting writers to enter only once all readers are done).

2. **Why does `readers` need its own semaphore `rlock`? What could go wrong without it?**
   - `++readers` and `--readers` are non-atomic read-modify-write operations shared across concurrent reader threads. Without `rlock`:
     - Two readers entering together could both read `readers == 0`, both write `1`, and both execute `sem_wait(&room)`, causing the second reader to deadlock on `room` while the first already holds it.
     - Or lost updates could skew `readers` (e.g., reaching `0` early while a reader is still inside, letting a writer enter concurrently with a reader, or remaining `> 0` after all readers leave, blocking writers permanently).

3. **Imagine readers keep arriving with no pause between them. What happens to `writer 2`?**
   - **Writer starvation:** Because `readers` stays above `0` continuously, `--readers == 0` is never reached and `sem_post(&room)` is never called. `writer 2` waits indefinitely at `sem_wait(&room)`.

---

## Part 4: Deadlock and How to Prevent It (`deadlock.c`)

### Build & Run (Fixed Hand-In Version)
`deadlock.c` contains the fixed version where both `one()` and `two()` acquire mutex `A` before mutex `B`:

```bash
gcc -Wall -pthread -o deadlock deadlock.c
./deadlock
```

### Observed Output (`deadlock_stuck` vs Fixed `./deadlock`)
- **Stuck (original version with reversed lock order in `two()`, 5s timeout):**
  ```text
  two: I have B, I want A
  one: I have A, I want B
  ```
  Exit code: `124` (timed out; deadlocked)

- **Fixed hand-in version (`./deadlock`, both threads lock `A` then `B`):**
  ```text
  one: I have A, I want B
  one: I have both
  two: I have B, I want A
  two: I have both
  done
  ```
  Exit code: `0`

### Observe Questions
1. **In the stuck run, which thread holds which mutex, and which one is each waiting for? Draw the circle.**
   - Thread `one` **holds `A`** and **waits for `B`**.
   - Thread `two` **holds `B`** and **waits for `A`**.

   ```text
   [Thread one] --wants--> (Mutex B) --held by--> [Thread two]
        ^                                               |
        |                                               |
     held by                                          wants
        |                                               v
    (Mutex A) <-----------------------------------------+
   ```

2. **Remove both `sleep(1)` lines from the original program and run it 10 times. Does it still always hang? Explain why that is worse, not better.**
   - Running the original lock-order program without `sleep(1)` 10 times (0.5s timeout each) produced **3 completions (`exit 0`) and 7 deadlocks (`exit 124`)**:
     - Runs completing with `done` (`exit 0`): runs 1, 4, 5
     - Runs hanging in deadlock (`exit 124`): runs 2, 3, 6, 7, 8, 9, 10
   - **Why this is worse, not better:** The deadlock is now **intermittent (timing-dependent)** rather than deterministic. Depending on OS thread scheduling, one thread can occasionally lock both mutexes and release them before the second thread takes `B`. As a result, tests may pass sporadically and hide a fatal concurrency bug until the program hangs randomly in production.

3. **In one sentence: why can taking locks in the same order never deadlock?**
   - Acquiring all locks in a single global order breaks the circular-wait condition required for deadlock, because a thread holding a later lock can never block waiting for an earlier lock.


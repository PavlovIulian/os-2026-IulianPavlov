
~ ❯ ls
Permissions Size User      Date Modified Name
drwxr-xr-x     - sunflower 19 Oct 15:33  󰲂 Documents
drwxr-xr-x     - sunflower 11 Nov 17:20  󰉍 Downloads
drwxr-xr-x     - sunflower 26 Oct 23:03   'lab crypt'
drwxr-xr-x     - sunflower 29 Sep 11:03   Marketplace
drwxr-xr-x     - sunflower 19 Oct 15:33  󱍙 Music
drwxr-xr-x     - sunflower 11 Nov 17:21   os-labs
drwxr-xr-x     - sunflower 29 Oct 12:30  󰉏 Pictures
drwxr-xr-x     - sunflower 19 Oct 15:33   Projects
drwxr-xr-x     - sunflower 19 Oct 10:39   ReteleLabs
drwxr-xr-x     - sunflower  7 Oct 14:29   TMPS
drwxr-xr-x     - sunflower 19 Oct 15:31   Videos
drwxr-xr-x     - sunflower 28 Sep 16:25   Work
drwxr-xr-x     - sunflower 11 Nov 16:42   xv6-riscv

~ ❯ cd os-labs

os-labs main ❯ cd lab11

os-labs/lab11 main ❯ ^[[200~cat > race.c << 'EOF'
∙ #include <stdio.h>
∙ #include <pthread.h>
∙ #define N 1000000
∙ long counter = 0;
∙ void *add(void *arg) {
∙     for (int i = 0; i < N; i++)
∙         counter++;
∙     return NULL;
∙ }
∙ int main(void) {
∙     pthread_t a, b;
∙     pthread_create(&a, NULL, add, NULL);
∙     pthread_create(&b, NULL, add, NULL);
∙     pthread_join(a, NULL);
∙     pthread_join(b, NULL);
∙     printf("counter = %ld (expected %d)\n", counter, 2 * N);
∙     return 0;
∙ }
∙ EOF
bash: command not found: cat

os-labs/lab11 main ? ✗ 

os-labs/lab11 main ? ✗ cat > mutex.c << 'EOF'
∙ #include <stdio.h>
∙ #include <pthread.h>
∙ #define N 1000000
∙ long counter = 0;
∙ pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
∙ void *add(void *arg) {
∙     for (int i = 0; i < N; i++) {
∙         pthread_mutex_lock(&lock);
∙         counter++;
∙         pthread_mutex_unlock(&lock);
∙     }
∙     return NULL;
∙ }
∙ int main(void) {
∙     pthread_t a, b;
∙     pthread_create(&a, NULL, add, NULL);
∙     pthread_create(&b, NULL, add, NULL);
∙     pthread_join(a, NULL);
∙     pthread_join(b, NULL);
∙     printf("counter = %ld (expected %d)\n", counter, 2 * N);
∙     return 0;
∙ }
∙ EOF

os-labs/lab11 main ? ❯ 

os-labs/lab11 main ? ❯ cat > sem.c << 'EOF'
∙ #include <stdio.h>
∙ #include <pthread.h>
∙ #include <semaphore.h>
∙ #define N 1000000
∙ long counter = 0;
∙ sem_t s;
∙ void *add(void *arg) {
∙     for (int i = 0; i < N; i++) {
∙         sem_wait(&s);
∙         counter++;
∙         sem_post(&s);
∙     }
∙     return NULL;
∙ }
∙ int main(void) {
∙     sem_init(&s, 0, 1);
∙     pthread_t a, b;
∙     pthread_create(&a, NULL, add, NULL);
∙     pthread_create(&b, NULL, add, NULL);
∙     pthread_join(a, NULL);
∙     pthread_join(b, NULL);
∙     printf("counter = %ld (expected %d)\n", counter, 2 * N);
∙     sem_destroy(&s);
∙     return 0;
∙ }
∙ EOF

os-labs/lab11 main ? ❯ 

os-labs/lab11 main ? ❯ cat > order.c << 'EOF'
∙ #include <stdio.h>
∙ #include <string.h>
∙ #include <unistd.h>
∙ #include <pthread.h>
∙ #include <semaphore.h>
∙ char message[32];
∙ sem_t ready;
∙ void *writer(void *arg) {
∙     sleep(1);
∙     strcpy(message, "hello");
∙     printf("A: message written\n");
∙     sem_post(&ready);
∙     return NULL;
∙ }
∙ void *reader(void *arg) {
∙     printf("B: waiting for the message...\n");
∙     sem_wait(&ready);
∙     printf("B: I read \"%s\"\n", message);
∙     return NULL;
∙ }
∙ int main(void) {
∙     sem_init(&ready, 0, 0);
∙     pthread_t a, b;
∙     pthread_create(&b, NULL, reader, NULL);
∙     pthread_create(&a, NULL, writer, NULL);
∙     pthread_join(a, NULL);
∙     pthread_join(b, NULL);
∙     sem_destroy(&ready);
∙     return 0;
∙ }
∙ EOF

os-labs/lab11 main ? ❯ 

os-labs/lab11 main ? ❯ cat > parking.c << 'EOF'
∙ #include <stdio.h>
∙ #include <unistd.h>
∙ #include <pthread.h>
∙ #include <semaphore.h>
∙ #define PLACES 3
∙ #define CARS 6
∙ sem_t places;
∙ pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
∙ int inside = 0;
∙ void *car(void *arg) {
∙     long id = (long)arg;
∙     sem_wait(&places);
∙     pthread_mutex_lock(&lock);
∙     inside++;
∙     printf("car %ld enters (inside: %d)\n", id, inside);
∙     pthread_mutex_unlock(&lock);
∙     sleep(1);
∙     pthread_mutex_lock(&lock);
∙     inside--;
∙     printf("car %ld leaves (inside: %d)\n", id, inside);
∙     pthread_mutex_unlock(&lock);
∙     sem_post(&places);
∙     return NULL;
∙ }
∙ int main(void) {
∙     sem_init(&places, 0, PLACES);
∙     pthread_t t[CARS];
∙     for (long i = 0; i < CARS; i++)
∙         pthread_create(&t[i], NULL, car, (void *)(i + 1));
∙     for (int i = 0; i < CARS; i++)
∙         pthread_join(t[i], NULL);
∙     sem_destroy(&places);
∙     return 0;
∙ }
∙ EOF

os-labs/lab11 main ? ❯ 

os-labs/lab11 main ? ❯ cat > atomic.c << 'EOF'
∙ #include <stdio.h>
∙ #include <pthread.h>
∙ #include <stdatomic.h>
∙ #define N 1000000
∙ atomic_long counter = 0;
∙ void *add(void *arg) {
∙     for (int i = 0; i < N; i++)
∙         atomic_fetch_add(&counter, 1);
∙     return NULL;
∙ }
∙ int main(void) {
∙     pthread_t a, b;
∙     pthread_create(&a, NULL, add, NULL);
∙     pthread_create(&b, NULL, add, NULL);
∙     pthread_join(a, NULL);
∙     pthread_join(b, NULL);
∙     printf("counter = %ld (expected %d)\n", (long)counter, 2 * N);
∙     return 0;
∙ }
∙ EOF

os-labs/lab11 main ? ❯ 

os-labs/lab11 main ? ❯ for p in race mutex sem order parking atomic; do gcc -Wall -pthread -o $p $p.c || echo "FAILED: $p"; done
/usr/bin/ld: /usr/lib/gcc/x86_64-pc-linux-gnu/16/../../../../lib/Scrt1.o: in function `_start':
(.text+0x1b): undefined reference to `main'
collect2: error: ld returned 1 exit status
FAILED: race

os-labs/lab11 main ? ❯ 

os-labs/lab11 main ? ❯ echo "--- Part 1 ---"; ./race; ./race; ./race
--- Part 1 ---
bash: ./race: No such file or directory
bash: ./race: No such file or directory
bash: ./race: No such file or directory

os-labs/lab11 main ? ✗ echo "--- Part 2 ---"; ./mutex; ./sem; time ./race; time ./mutex; time ./sem
--- Part 2 ---
counter = 2000000 (expected 2000000)
counter = 2000000 (expected 2000000)
bash: ./race: No such file or directory

real	0m0.001s
user	0m0.000s
sys	0m0.001s
counter = 2000000 (expected 2000000)

real	0m0.197s
user	0m0.204s
sys	0m0.180s
counter = 2000000 (expected 2000000)

real	0m0.235s
user	0m0.213s
sys	0m0.245s

os-labs/lab11 main ? ❯ echo "--- Part 3 ---"; ./order
--- Part 3 ---
B: waiting for the message...
A: message written
B: I read "hello"

os-labs/lab11 main ? ❯ echo "--- Part 4 ---"; ./parking~
--- Part 4 ---
bash: ./parking~: No such file or directory

os-labs/lab11 main ? ✗ 

os-labs/lab11 main ? ✗ sed 's/sem_init(&s, 0, 1)/sem_init(\&s, 0, 2)/' sem.c > sem2.c

os-labs/lab11 main ? ❯ gcc -Wall -pthread -o sem2 sem2.c && ./sem2 && ./sem2 && ./sem2
counter = 1610383 (expected 2000000)
counter = 1914526 (expected 2000000)
counter = 1386239 (expected 2000000)

os-labs/lab11 main ? ❯ sed 's/sem_init(&ready, 0, 0)/sem_init(\&ready, 0, 1)/' order.c > order1.c

os-labs/lab11 main ? ❯ gcc -Wall -pthread -o order1 order1.c && ./order1
B: waiting for the message...
B: I read ""
A: message written

os-labs/lab11 main ? ❯ sed 's/#define PLACES 3/#define PLACES 1/' parking.c > parking1.c

os-labs/lab11 main ? ❯ gcc -Wall -pthread -o parking1 parking1.c && ./parking1
car 1 enters (inside: 1)
car 1 leaves (inside: 0)
car 2 enters (inside: 1)
car 2 leaves (inside: 0)
car 4 enters (inside: 1)
car 4 leaves (inside: 0)
car 6 enters (inside: 1)
car 6 leaves (inside: 0)
car 3 enters (inside: 1)
car 3 leaves (inside: 0)
car 5 enters (inside: 1)
car 5 leaves (inside: 0)
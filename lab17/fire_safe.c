/* fire_safe.c - safe counter increment using pthread_mutex (proposing the fix)
 * Build: gcc -Wall -Wextra -pthread -o fire_safe fire_safe.c Run: ./fire_safe
 */
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

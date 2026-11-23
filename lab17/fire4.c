
#include <stdio.h>
#include <pthread.h>

#define N 1000000
#define NUM_THREADS 4
long counter = 0; /* one variable, shared by all 4 threads */

void *work(void *arg) {
    long id = (long)arg;
    printf("thread %ld sees counter = %ld\n", id, counter);
    for (int i = 0; i < N; i++)
        counter++; /* NOT atomic: read, add, write */
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

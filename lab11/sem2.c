#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#define N 1000000
long counter = 0;
sem_t s;
void *add(void *arg) {
    for (int i = 0; i < N; i++) {
        sem_wait(&s);
        counter++;
        sem_post(&s);
    }
    return NULL;
}
int main(void) {
    sem_init(&s, 0, 2);
    pthread_t a, b;
    pthread_create(&a, NULL, add, NULL);
    pthread_create(&b, NULL, add, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("counter = %ld (expected %d)\n", counter, 2 * N);
    sem_destroy(&s);
    return 0;
}

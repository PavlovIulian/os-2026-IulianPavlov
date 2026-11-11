#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#define PLACES 3
#define CARS 6
sem_t places;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int inside = 0;
void *car(void *arg) {
    long id = (long)arg;
    sem_wait(&places);
    pthread_mutex_lock(&lock);
    inside++;
    printf("car %ld enters (inside: %d)\n", id, inside);
    pthread_mutex_unlock(&lock);
    sleep(1);
    pthread_mutex_lock(&lock);
    inside--;
    printf("car %ld leaves (inside: %d)\n", id, inside);
    pthread_mutex_unlock(&lock);
    sem_post(&places);
    return NULL;
}
int main(void) {
    sem_init(&places, 0, PLACES);
    pthread_t t[CARS];
    for (long i = 0; i < CARS; i++)
        pthread_create(&t[i], NULL, car, (void *)(i + 1));
    for (int i = 0; i < CARS; i++)
        pthread_join(t[i], NULL);
    sem_destroy(&places);
    return 0;
}

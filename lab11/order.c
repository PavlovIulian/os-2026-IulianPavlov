#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
char message[32];
sem_t ready;
void *writer(void *arg) {
    sleep(1);
    strcpy(message, "hello");
    printf("A: message written\n");
    sem_post(&ready);
    return NULL;
}
void *reader(void *arg) {
    printf("B: waiting for the message...\n");
    sem_wait(&ready);
    printf("B: I read \"%s\"\n", message);
    return NULL;
}
int main(void) {
    sem_init(&ready, 0, 0);
    pthread_t a, b;
    pthread_create(&b, NULL, reader, NULL);
    pthread_create(&a, NULL, writer, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    sem_destroy(&ready);
    return 0;
}

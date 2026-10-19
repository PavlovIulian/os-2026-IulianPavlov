    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>

    int main(void)
    {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            printf("Child (PID %d) is exiting now...\n", getpid());
            exit(0);
        } else {
            printf("Parent (PID %d) created child (PID %d).\n", getpid(), pid);
            printf("Parent sleeping for 30 seconds. Check `ps` in another terminal!\n");
            sleep(30);
            printf("Parent waking up and exiting.\n");
        }

        return 0;
    }

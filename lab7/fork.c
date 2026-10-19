    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/wait.h>

    int main(void)
    {
        printf("before fork: pid=%d\n", getpid());
        /* fflush(stdout); removed */

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) { /* child */
            printf("child: pid=%d parent=%d\n", getpid(), getppid());
            exit(42);
        }

        int status; /* parent */
        waitpid(pid, &status, 0);
        printf("parent: pid=%d, my child %d exited with %d\n",
               getpid(), pid, WEXITSTATUS(status));
        return 0;
    }

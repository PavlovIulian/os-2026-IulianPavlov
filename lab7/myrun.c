  #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/wait.h>

    /* usage: ./myrun program [args...] e.g. ./myrun ls -l /tmp */
    int main(int argc, char *argv[])
    {
        if (argc < 2) {
            fprintf(stderr, "usage: %s program [args...]\n", argv[0]);
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            /* TODO 1: replace this process with argv[1], passing &argv[1]
               as its argument list (use execvp: it searches PATH) */
            execvp(argv[1], &argv[1]);

            /* TODO 2: if you get here, exec failed: perror and exit(127) */
            perror("execvp");
            exit(127);
        }

        int status;
        /* TODO 3: wait for the child */
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }

        /* TODO 4: if it exited normally (WIFEXITED), print
           [myrun] <prog> exited with status <n>
           if it was killed by a signal (WIFSIGNALED), print
           [myrun] <prog> killed by signal <n> (WTERMSIG) */
        if (WIFEXITED(status)) {
            printf("[myrun] %s exited with status %d\n", argv[1], WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[myrun] %s killed by signal %d\n", argv[1], WTERMSIG(status));
        }

        return 0;
    }

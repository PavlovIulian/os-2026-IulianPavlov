 #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/wait.h>

    int main(void)
    {
        int p1[2], p2[2];
        if (pipe(p1) < 0 || pipe(p2) < 0) {
            perror("pipe");
            exit(1);
        }

        /* Stage 1: ls /usr/bin -> p1[1] */
        if (fork() == 0) {
            dup2(p1[1], STDOUT_FILENO);
            close(p1[0]); close(p1[1]);
            close(p2[0]); close(p2[1]);
            execlp("ls", "ls", "/usr/bin", (char *)NULL);
            exit(127);
        }

        /* Stage 2: p1[0] -> grep x -> p2[1] */
        if (fork() == 0) {
            dup2(p1[0], STDIN_FILENO);
            dup2(p2[1], STDOUT_FILENO);
            close(p1[0]); close(p1[1]);
            close(p2[0]); close(p2[1]);
            execlp("grep", "grep", "x", (char *)NULL);
            exit(127);
        }

        /* Stage 3: p2[0] -> wc -l */
        if (fork() == 0) {
            dup2(p2[0], STDIN_FILENO);
            close(p1[0]); close(p1[1]);
            close(p2[0]); close(p2[1]);
            execlp("wc", "wc", "-l", (char *)NULL);
            exit(127);
        }

        /* Parent closes all pipe ends */
        close(p1[0]); close(p1[1]);
        close(p2[0]); close(p2[1]);

        /* Wait for all 3 children */
        while (wait(NULL) > 0);

        return 0;
    }

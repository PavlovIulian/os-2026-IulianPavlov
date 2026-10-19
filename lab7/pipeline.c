 #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/wait.h>

    int main(void)
    {
        int p[2];
        if (pipe(p) < 0) {
            perror("pipe");
            exit(1);
        }

        /* Child 1: ls -l /etc */
        pid_t pid1 = fork();
        if (pid1 < 0) {
            perror("fork");
            exit(1);
        }

        if (pid1 == 0) {
            close(p[0]);            /* not reading */
            dup2(p[1], STDOUT_FILENO); /* redirect stdout to pipe write end */
            close(p[1]);

            execlp("ls", "ls", "-l", "/etc", (char *)NULL);
            perror("execlp ls");
            exit(127);
        }

        /* Child 2: wc -l */
        pid_t pid2 = fork();
        if (pid2 < 0) {
            perror("fork");
            exit(1);
        }

        if (pid2 == 0) {
            close(p[1]);            /* not writing */
            dup2(p[0], STDIN_FILENO);  /* redirect stdin to pipe read end */
            close(p[0]);

            execlp("wc", "wc", "-l", (char *)NULL);
            perror("execlp wc");
            exit(127);
        }

        /* Parent closes both pipe ends so EOF can be reached */
        close(p[0]);
        close(p[1]);

        /* Wait for both children */
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);

        return 0;
    }

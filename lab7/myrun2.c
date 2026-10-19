 #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/wait.h>

    /* usage: ./myrun2 outfile program [args...] */
    int main(int argc, char *argv[])
    {
        if (argc < 3) {
            fprintf(stderr, "usage: %s outfile program [args...]\n", argv[0]);
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);

            execvp(argv[2], &argv[2]);
            perror("execvp");
            exit(127);
        }

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("[myrun2] %s exited with status %d (output written to %s)\n",
                   argv[2], WEXITSTATUS(status), argv[1]);
        } else if (WIFSIGNALED(status)) {
            printf("[myrun2] %s killed by signal %d\n", argv[2], WTERMSIG(status));
        }

        return 0;
    }

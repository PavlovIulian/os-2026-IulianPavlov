 #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/wait.h>

    int main(void)
    {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            /* Child: redirect stdout (fd 1) to out.txt */
            int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open");
                exit(1);
            }

            dup2(fd, 1);    /* stdout now points to out.txt */
            close(fd);      /* close original fd; descriptor 1 remains open */

            execlp("ls", "ls", "-l", (char *)NULL);
            perror("execlp");
            exit(127);
        }

        /* Parent */
        int status;
        waitpid(pid, &status, 0);
        return 0;
    }

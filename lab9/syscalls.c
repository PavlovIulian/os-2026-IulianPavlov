/* syscalls.c - open, write, read and close, using only system calls.
 * Build:  gcc -Wall -Wextra -o syscalls syscalls.c
 * Run:    ./syscalls
 * See the calls: strace ./syscalls
 */
#include <fcntl.h>   /* open */
#include <unistd.h>  /* write, read, close */
#include <string.h>  /* strlen */
#include <stdio.h>   /* printf */

int main(void) {
    const char *msg = "hello, kernel\n";

    /* open() is a system call: ask the kernel for a file, get back a descriptor */
    int fd = open("note.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return 1; }
    printf("open returned descriptor %d\n", fd);

    /* write() is a system call: hand the bytes to the kernel */
    ssize_t n = write(fd, msg, strlen(msg));
    if (n < 0) { perror("write"); close(fd); return 1; }
    printf("write sent %zd bytes\n", n);
    close(fd);

    /* read it back, through the kernel again */
    fd = open("note.txt", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    char buf[64];
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) { perror("read"); close(fd); return 1; }
    buf[n] = '\0';
    printf("read got %zd bytes: %s", n, buf);
    close(fd);

    return 0;
}

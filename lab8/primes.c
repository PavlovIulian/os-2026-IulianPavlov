#include "kernel/types.h"
#include "user/user.h"

#define LIMIT 35

void stage(int) __attribute__((noreturn));

void
stage(int in)
{
  int p, n, fd[2], pid;

  if(read(in, &p, sizeof(p)) != sizeof(p)){
    close(in);
    exit(0);
  }
  printf("prime %d\n", p);

  if(pipe(fd) < 0){
    printf("primes: pipe failed after prime %d\n", p);
    close(in);
    exit(1);
  }
  pid = fork();
  if(pid < 0){
    printf("primes: fork failed after prime %d\n", p);
    close(fd[0]); close(fd[1]); close(in);
    exit(1);
  }
  if(pid == 0){
    close(fd[1]);
    close(in);
    stage(fd[0]);
  }
  close(fd[0]);
  while(read(in, &n, sizeof(n)) == sizeof(n)){
    if(n % p != 0)
      write(fd[1], &n, sizeof(n));
  }
  close(in);
  close(fd[1]);
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int fd[2];

  if(pipe(fd) < 0)
    exit(1);
  if(fork() == 0){
    close(fd[1]);
    stage(fd[0]);
  }
  close(fd[0]);
  for(int i = 2; i <= LIMIT; i++)
    write(fd[1], &i, sizeof(i));
  close(fd[1]);
  wait(0);
  exit(0);
}

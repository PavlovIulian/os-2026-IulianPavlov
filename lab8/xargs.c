#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[512], *args[MAXARG], *s;
  int n, r, k, i;
  char c;

  for(i = 1; i < argc && i < MAXARG - 1; i++)
    args[i-1] = argv[i];
  int base = i - 1;

  for(;;){
    n = 0;
    while((r = read(0, &c, 1)) == 1 && c != '\n'){
      if(n < sizeof(buf) - 1)
        buf[n++] = c;
    }
    if(r != 1 && n == 0)
      break;
    buf[n] = 0;

    k = base;
    s = buf;
    while(*s && k < MAXARG - 1){
      while(*s == ' ') *s++ = 0;
      if(*s == 0) break;
      args[k++] = s;
      while(*s && *s != ' ') s++;
    }
    args[k] = 0;

    if(fork() == 0){
      exec(args[0], args);
      fprintf(2, "xargs: exec %s failed\n", args[0]);
      exit(1);
    }
    wait(0);
    if(r != 1)
      break;
  }
  exit(0);
}

#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "usage: sleep <ticks>\n");
    exit(1);
  }
  
  // Note: Upstream MIT xv6 renamed sleep() to pause() in Aug 2025
  // to avoid name clashes with the kernel internal sleep(chan, lock).
  pause(atoi(argv[1]));
  exit(0);
}

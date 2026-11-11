// trydown.c: demonstrate non-blocking sem_trydown() returning 0 then -1.
#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  sem_init(4, 1);
  printf("first trydown: %d\n", sem_trydown(4));
  printf("second trydown (at 0): %d\n", sem_trydown(4));
  sem_up(4);
  printf("after sem_up, third trydown: %d\n", sem_trydown(4));
  exit(0);
}


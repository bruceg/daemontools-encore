#include "alloc.h"
#include "fifo.h"
#include "stralloc.h"
#include "strerr.h"

#define FATAL "makefifo: fatal: "
#define WARNING "makefifo: warning: "

static void die_usage(void)
{
  strerr_die1x(100,"makefifo: usage: makefifo fifo1 [fifo2 ...]");
}

static void die_nomem(void)
{
  strerr_die2sys(100,FATAL,"out of memory");
}

int main(int argc, char **argv)
{
  stralloc fn = {0,0,0};
  int i;

  if (argc < 2) die_usage();

  for (i = 1;i < argc; ++i) {
    if (!stralloc_copys(&fn,argv[i])) die_nomem();
    if (!stralloc_0(&fn)) die_nomem();
    if (fifo_make(fn.s,0600))
      strerr_warn3sys(WARNING,"can't make fifo ",fn.s);
  }

  alloc_free(fn.s);
  return 0;
}

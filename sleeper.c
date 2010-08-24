#include "sig.h"
#include <unistd.h>

static void catch_term(int sig)
{
  (void)sig;
  write(1, "Caught TERM\n", 12);
  _exit(1);
}

int main(void)
{
  sig_catch(sig_term,catch_term);
  sleep(9999);
  return 0;
}

#include <signal.h>
#include "sig.h"
#include "str.h"
#include <unistd.h>

static void catch_sig(int sig)
{
  const char *name;
  int ignored;
  switch (sig) {
  case SIGALRM: name = "ALRM"; break;
  case SIGCONT: name = "CONT"; break;
  case SIGHUP: name = "HUP"; break;
  case SIGINT: name = "INT"; break;
  case SIGQUIT: name = "QUIT"; break;
  case SIGTERM: name = "TERM"; break;
  case SIGUSR1: name = "USR1"; break;
  case SIGUSR2: name = "USR2"; break;
  case SIGWINCH: name = "WINCH"; break;
  default: name = "unknown signal";
  }
  ignored = write(1, "Caught ", 7);
  ignored = write(1, name, str_len(name));
  ignored = write(1, "\n", 1);
  if (sig != SIGCONT)
    _exit(1);
}

int main(void)
{
  sig_catch(SIGALRM,catch_sig);
  sig_catch(SIGCONT,catch_sig);
  sig_catch(SIGHUP,catch_sig);
  sig_catch(SIGINT,catch_sig);
  sig_catch(SIGQUIT,catch_sig);
  sig_catch(SIGTERM,catch_sig);
  sig_catch(SIGUSR1,catch_sig);
  sig_catch(SIGUSR2,catch_sig);
  sig_catch(SIGWINCH,catch_sig);
  sleep(9999);
  return 0;
}

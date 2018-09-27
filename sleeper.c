#include <signal.h>
#include "byte.h"
#include "sig.h"
#include "str.h"
#include <unistd.h>

static void catch_sig(int sig)
{
  char buf[7+14+2] = "Caught ";
  const char *name;
  int ignored;
  int i;
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
  i = str_len(name);
  byte_copy(buf+7,i,name);
  i += 7;
  buf[i++] = '\n';
  ignored = write(1,buf,i);
  if (sig != SIGCONT)
    _exit(1);
  (void)ignored;
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

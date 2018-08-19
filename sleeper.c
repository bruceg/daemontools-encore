#include <signal.h>
#include "byte.h"
#include "sig.h"
#include "str.h"
#include <unistd.h>

int flag_cont = 0;
int flag_term = 0;

static void catch_sig(int sig)
{
  char buf[7+14+2] = "Caught ";
  const char *name;
  int ignored;
  int i;
  switch (sig) {
  case SIGALRM: name = "ALRM"; break;
  case SIGCONT: flag_cont = 1; return;
  case SIGHUP: name = "HUP"; break;
  case SIGINT: name = "INT"; break;
  case SIGQUIT: name = "QUIT"; break;
  case SIGTERM: flag_term = 1; return;
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
  _exit(1);
}

int main(void)
{
  int i, ignored;

  sig_catch(SIGALRM,catch_sig);
  sig_catch(SIGCONT,catch_sig);
  sig_catch(SIGHUP,catch_sig);
  sig_catch(SIGINT,catch_sig);
  sig_catch(SIGQUIT,catch_sig);
  sig_catch(SIGTERM,catch_sig);
  sig_catch(SIGUSR1,catch_sig);
  sig_catch(SIGUSR2,catch_sig);
  sig_catch(SIGWINCH,catch_sig);

  for (i = 0; i < 9999; ++i) {
    if (!flag_cont && !flag_term) sleep(1);
    /*
     * here we have some combination of flag_cont and flag_term.
     *
     * if neither, nothing happened. continue.
     *
     * if both, then sig_term was interrupted by sig_cont, so we know
     * we've handled svc -d. print both and exit.
     *
     * if only flag_cont set, we know we're handling svc -c since svc -d
     * would've either set both (as above) or only set flag_term (below).
     * print the cont message and exit.
     *
     * if only flag_term set, we might be handling svc -t by itself, or
     * maybe svc -d has returned from the sig_term handler before sig_cont
     * arrived. in either case, reset flag_term, print the term message,
     * and continue.
     *
     * as a special case, if only flag_cont set, we might be handling
     * the second half of svc -d where sig_cont arrived so late we already
     * looped around here once and handled the sig_term. print the cont
     * message and exit.
     *
     * one hidden gotcha here is that any test following a sig_term will
     * be handled by the same sleeper instance as sig_term itself. don't
     * schedule a svc -t test last.
     *
     * another gotcha is the race condition between the flag test above
     * and sleep(). if a cont/term signal arrives at that point, we incur
     * additional delay which could muck up the tests. don't test svc -c
     * immediately after svc -t, and schedule svc -dx test last.
     */
    if (flag_term) {
      flag_term = 0;
      ignored = write(1, "Caught TERM\n", 12);
    }
    if (flag_cont) {
      ignored = write(1, "Caught CONT\n", 12);
      _exit(1);
    }
  }
  return 0;
}

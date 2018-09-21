#include <signal.h>
#include "byte.h"
#include "sig.h"
#include "str.h"
#include <unistd.h>

#define SIGQLEN 64

static unsigned int sig_ptr = 0;
static int sig_queue[SIGQLEN];

static void catch_sig(int sig)
{
  int ignored;
  sig_queue[sig_ptr++] = sig;
  if (sig_ptr < SIGQLEN) return;
  ignored = write(1,"increase SIGQLEN\n",17);
  _exit(2);
}

static void show_sig(int sig)
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
}

static void show_err()
{
  int ignored;
  ignored = write(1,"invalid signal\n",15);
  _exit(1);
}

static void show_one(int sig)
{
  show_sig(sig);
  _exit(1);
}

static void show_two(int sig1, int sig2)
{
  show_sig(sig1);
  show_sig(sig2);
  _exit(1);
}

#define new_cont (new_sig == SIGCONT)
#define new_term (new_sig == SIGTERM)
#define old_cont (old_sig == SIGCONT)
#define old_term (old_sig == SIGTERM)

#define state_00 ( ! new_cont && ! new_term && ! old_cont && ! old_term)
#define state_01 ( ! new_cont && ! new_term && ! old_cont &&   old_term)
#define state_02 ( ! new_cont && ! new_term &&   old_cont && ! old_term)
#define state_03 ( ! new_cont && ! new_term &&   old_cont &&   old_term)
#define state_04 ( ! new_cont &&   new_term && ! old_cont && ! old_term)
#define state_05 ( ! new_cont &&   new_term && ! old_cont &&   old_term)
#define state_06 ( ! new_cont &&   new_term &&   old_cont && ! old_term)
#define state_07 ( ! new_cont &&   new_term &&   old_cont &&   old_term)
#define state_08 (   new_cont && ! new_term && ! old_cont && ! old_term)
#define state_09 (   new_cont && ! new_term && ! old_cont &&   old_term)
#define state_10 (   new_cont && ! new_term &&   old_cont && ! old_term)
#define state_11 (   new_cont && ! new_term &&   old_cont &&   old_term)
#define state_12 (   new_cont &&   new_term && ! old_cont && ! old_term)
#define state_13 (   new_cont &&   new_term && ! old_cont &&   old_term)
#define state_14 (   new_cont &&   new_term &&   old_cont && ! old_term)
#define state_15 (   new_cont &&   new_term &&   old_cont &&   old_term)

int main(void)
{
  int i, new_sig, old_sig=0;

  sig_catch(SIGALRM,catch_sig);
  sig_catch(SIGCONT,catch_sig);
  sig_catch(SIGHUP,catch_sig);
  sig_catch(SIGINT,catch_sig);
  sig_catch(SIGQUIT,catch_sig);
  sig_catch(SIGTERM,catch_sig);
  sig_catch(SIGUSR1,catch_sig);
  sig_catch(SIGUSR2,catch_sig);
  sig_catch(SIGWINCH,catch_sig);

  for (i = 0; i < SIGQLEN; ++i) {
    while (i >= sig_ptr) sleep(9999);
    new_sig = sig_queue[i];
    if (i) old_sig = sig_queue[i-1];
    if (state_00) show_one(new_sig);
    if (state_01) show_two(old_sig, new_sig);
    if (state_02) show_two(old_sig, new_sig);
    if (state_03) show_err();
    if (state_04) continue;
    if (state_05) show_one(old_sig);
    if (state_06) show_two(old_sig, new_sig);
    if (state_07) show_err();
    if (state_08) continue;
    if (state_09) show_two(new_sig, old_sig);
    if (state_10) show_one(old_sig);
    if (state_11) show_err();
    if (state_12) show_err();
    if (state_13) show_err();
    if (state_14) show_err();
    if (state_15) show_err();
  }
  return 0;
}

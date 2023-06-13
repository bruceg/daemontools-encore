/* Public domain. */

#include <signal.h>
#include "sig.h"
#include "hassgprm.h"

// Use POSIX complaint functions when using non-Glibc system
#ifndef __GLIBC__
#define HASSIGPROCMASK 0
#endif

void sig_block(int sig)
{
#ifdef HASSIGPROCMASK
  sigset_t ss;
  sigemptyset(&ss);
  sigaddset(&ss,sig);
  sigprocmask(SIG_BLOCK,&ss,(sigset_t *) 0);
#else
  sigblock(1 << (sig - 1));
#endif
}

void sig_unblock(int sig)
{
#ifdef HASSIGPROCMASK
  sigset_t ss;
  sigemptyset(&ss);
  sigaddset(&ss,sig);
  sigprocmask(SIG_UNBLOCK,&ss,(sigset_t *) 0);
#else
  sigsetmask(sigsetmask(~0) & ~(1 << (sig - 1)));
#endif
}

void sig_blocknone(void)
{
#ifdef HASSIGPROCMASK
  sigset_t ss;
  sigemptyset(&ss);
  sigprocmask(SIG_SETMASK,&ss,(sigset_t *) 0);
#else
  sigsetmask(0);
#endif
}

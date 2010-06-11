#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include "sig.h"
#include "strerr.h"
#include "error.h"
#include "fifo.h"
#include "open.h"
#include "lock.h"
#include "wait.h"
#include "closeonexec.h"
#include "ndelay.h"
#include "env.h"
#include "iopause.h"
#include "taia.h"
#include "deepsleep.h"
#include "stralloc.h"
#include "svpath.h"
#include "svstatus.h"

#define FATAL "supervise: fatal: "
#define WARNING "supervise: warning: "

const char *dir;
stralloc fn_status = {0};
stralloc fn_status_new = {0};
int selfpipe[2];
int fdlock;
int fdcontrolwrite;
int fdcontrol;
int fdok;

int flagexit = 0;
int flagwant = 1;
int flagwantup = 1;
int pid = 0; /* 0 means down */
int flagpaused; /* defined if(pid) */
int firstrun = 1;
enum svstatus flagstatus = svstatus_starting;

char status[19];

static void die_nomem(void)
{
  strerr_die2sys(111,FATAL,"unable to allocate memory");
}

void pidchange(void)
{
  struct taia now;
  unsigned long u;

  taia_now(&now);
  taia_pack(status,&now);

  u = (unsigned long) pid;
  status[12] = u; u >>= 8;
  status[13] = u; u >>= 8;
  status[14] = u; u >>= 8;
  status[15] = u;
}

void announce(void)
{
  int fd;
  int r;

  status[16] = (pid ? flagpaused : 0);
  status[17] = (flagwant ? (flagwantup ? 'u' : 'd') : 0);
  status[18] = flagstatus;

  fd = open_trunc(fn_status_new.s);
  if (fd == -1) {
    strerr_warn4(WARNING,"unable to open ",fn_status_new.s,": ",&strerr_sys);
    return;
  }

  r = write(fd,status,sizeof status);
  if (r == -1) {
    strerr_warn4(WARNING,"unable to write ",fn_status_new.s,": ",&strerr_sys);
    close(fd);
    return;
  }
  close(fd);
  if (r < sizeof status) {
    strerr_warn4(WARNING,"unable to write ",fn_status_new.s,": partial write",0);
    return;
  }

  if (rename(fn_status_new.s,fn_status.s) == -1)
    strerr_warn4(WARNING,"unable to rename ",fn_status_new.s," to status: ",&strerr_sys);
}

void trigger(void)
{
  write(selfpipe[1],"",1);
}

const char *run[2] = { 0,0 };

void trystart(void)
{
  int f;

  if (firstrun && access("start", X_OK) != 0)
    firstrun = 0;
  run[0] = firstrun ? "./start" : "./run";
  flagstatus = firstrun ? svstatus_starting : svstatus_running;
  switch(f = fork()) {
    case -1:
      strerr_warn4(WARNING,"unable to fork for ",dir,", sleeping 60 seconds: ",&strerr_sys);
      deepsleep(60);
      trigger();
      return;
    case 0:
      sig_uncatch(sig_child);
      sig_unblock(sig_child);
      setsid();			/* shouldn't fail; if it does, too bad */
      execve(*run,run,environ);
      strerr_die5sys(111,FATAL,"unable to start ",dir,run[0]+1,": ");
  }
  flagpaused = 0;
  pid = f;
  pidchange();
  announce();
  deepsleep(1);
}

void doit(void)
{
  iopause_fd x[2];
  struct taia deadline;
  struct taia stamp;
  int wstat;
  int r;
  char ch;
  int killpid;

  announce();

  for (;;) {
    if (flagexit && !pid) return;

    sig_unblock(sig_child);

    x[0].fd = selfpipe[0];
    x[0].events = IOPAUSE_READ;
    x[1].fd = fdcontrol;
    x[1].events = IOPAUSE_READ;
    taia_now(&stamp);
    taia_uint(&deadline,3600);
    taia_add(&deadline,&stamp,&deadline);
    iopause(x,2,&deadline,&stamp);

    sig_block(sig_child);

    while (read(selfpipe[0],&ch,1) == 1)
      ;

    for (;;) {
      r = wait_nohang(&wstat);
      if (!r) break;
      if ((r == -1) && (errno != error_intr)) break;
      if (r == pid) {
	pid = 0;
	if (!wait_crashed(wstat) && wait_exitcode(wstat) == 100) {
	  flagwantup = 0;
	  flagstatus = svstatus_failed;
	}
	pidchange();
	announce();
	firstrun = 0;
	if (flagexit) return;
	if (flagwant && flagwantup) trystart();
	break;
      }
    }

    killpid = pid;
    while (read(fdcontrol,&ch,1) == 1)
      switch(ch) {
        case '+':
	  killpid = -pid;
	  break;
	case 'd':
	  flagwant = 1;
	  flagwantup = 0;
	  if (pid) {
	    kill(killpid,SIGTERM);
	    kill(killpid,SIGCONT);
	    flagpaused = 0;
	    flagstatus = svstatus_stopping;
	  }
	  else
	    flagstatus = svstatus_stopped;
	  announce();
	  break;
	case 'u':
	  firstrun = !flagwantup;
	  flagwant = 1;
	  flagwantup = 1;
	  if (!pid) flagstatus = svstatus_starting;
	  announce();
	  if (!pid) trystart();
	  break;
	case 'o':
	  flagwant = 0;
	  announce();
	  if (!pid) trystart();
	  break;
	case 'a':
	  if (pid) kill(killpid,SIGALRM);
	  break;
	case 'h':
	  if (pid) kill(killpid,SIGHUP);
	  break;
	case 'k':
	  if (pid) kill(killpid,SIGKILL);
	  break;
	case 't':
	  if (pid) kill(killpid,SIGTERM);
	  break;
	case 'i':
	  if (pid) kill(killpid,SIGINT);
	  break;
	case 'q':
	  if (pid) kill(killpid,SIGQUIT);
	  break;
	case '1':
	  if (pid) kill(killpid,SIGUSR1);
	  break;
	case '2':
	  if (pid) kill(killpid,SIGUSR2);
	  break;
	case 'p':
	  flagpaused = 1;
	  announce();
	  if (pid) kill(killpid,SIGSTOP);
	  break;
	case 'c':
	  flagpaused = 0;
	  announce();
	  if (pid) kill(killpid,SIGCONT);
	  break;
	case 'x':
	  flagexit = 1;
	  announce();
	  break;
      }
  }
}

void make_svpaths(void)
{
  if (!svpath_copy(&fn_status,"/status")
      || !svpath_copy(&fn_status_new,"/status.new"))
    die_nomem();
}

int main(int argc,char **argv)
{
  struct stat st;
  const char *fntemp;

  dir = argv[1];
  if (!dir || argv[2])
    strerr_die1x(100,"supervise: usage: supervise dir");

  if (pipe(selfpipe) == -1)
    strerr_die4sys(111,FATAL,"unable to create pipe for ",dir,": ");
  closeonexec(selfpipe[0]);
  closeonexec(selfpipe[1]);
  ndelay_on(selfpipe[0]);
  ndelay_on(selfpipe[1]);

  sig_block(sig_child);
  sig_catch(sig_child,trigger);

  if (chdir(dir) == -1)
    strerr_die4sys(111,FATAL,"unable to chdir to ",dir,": ");
  if (!svpath_init())
    strerr_die4sys(111,FATAL,"unable to setup control path for ",dir,": ");
  make_svpaths();

  if (stat("down",&st) != -1)
    flagwantup = 0;
  else
    if (errno != error_noent)
      strerr_die4sys(111,FATAL,"unable to stat ",dir,"/down: ");

  if ((fntemp = svpath_make("")) == 0) die_nomem();
  if (mkdir(fntemp,0700) != 0 && errno != error_exist)
    strerr_die4sys(111,FATAL,"unable to create ",fntemp,": ");
  if ((fntemp = svpath_make("/lock")) == 0) die_nomem();
  fdlock = open_append(fntemp);
  if ((fdlock == -1) || (lock_exnb(fdlock) == -1))
    strerr_die4sys(111,FATAL,"unable to acquire ",fntemp,": ");
  closeonexec(fdlock);

  if ((fntemp = svpath_make("/control")) == 0) die_nomem();
  fifo_make(fntemp,0600);
  fdcontrol = open_read(fntemp);
  if (fdcontrol == -1)
    strerr_die4sys(111,FATAL,"unable to read ",fntemp,": ");
  closeonexec(fdcontrol);
  ndelay_on(fdcontrol); /* shouldn't be necessary */
  fdcontrolwrite = open_write(fntemp);
  if (fdcontrolwrite == -1)
    strerr_die4sys(111,FATAL,"unable to write ",fntemp,": ");
  closeonexec(fdcontrolwrite);

  pidchange();
  announce();

  if ((fntemp = svpath_make("/ok")) == 0) die_nomem();
  fifo_make(fntemp,0600);
  fdok = open_read(fntemp);
  if (fdok == -1)
    strerr_die4sys(111,FATAL,"unable to read ",fntemp,": ");
  closeonexec(fdok);

  if (!flagwant || flagwantup) trystart();

  doit();
  announce();
  _exit(0);
}

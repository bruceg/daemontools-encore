#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include "sig.h"
#include "strerr.h"
#include "error.h"
#include "fifo.h"
#include "fmt.h"
#include "open.h"
#include "lock.h"
#include "seek.h"
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

struct svc
{
  int pid;
  enum svstatus flagstatus;
  int flagwant;
  int flagwantup;
  int flagpaused;
  struct taia when;
};

const char *dir;
int selfpipe[2];
int fdlock;
int fdcontrolwrite;
int fdcontrol;
int fdok;
int fdstatus;

int flagexit = 0;
int firstrun = 1;
const char *runscript = 0;

int logpipe[2] = {-1,-1};
struct svc svcmain = {0,svstatus_starting,1,1};
struct svc svclog = {0,svstatus_starting,1,0};

static int stat_isexec(const char *path)
{
  struct stat st;
  if (stat(path,&st) == -1)
    return errno == error_noent ? 0 : -1;
  return S_ISREG(st.st_mode) && (st.st_mode & 0100);
}

static void die_nomem(void)
{
  strerr_die2sys(111,FATAL,"unable to allocate memory");
}

static void trigger(void)
{
  write(selfpipe[1],"",1);
}

static int forkexecve(const char *argv[],int fd)
{
  int f;

  switch (f = fork()) {
    case -1:
      strerr_warn4(WARNING,"unable to fork for ",dir,", sleeping 60 seconds: ",&strerr_sys);
      deepsleep(60);
      trigger();
      return -1;
    case 0:
      sig_uncatch(sig_child);
      sig_unblock(sig_child);
      setsid();			/* shouldn't fail; if it does, too bad */
      if (fd >= 0 && logpipe[0] >= 0) {
	dup2(logpipe[fd],fd);
	close(logpipe[0]);
	close(logpipe[1]);
      }
      execve(argv[0],argv,environ);
      strerr_die5sys(111,FATAL,"unable to start ",dir,argv[0]+1,": ");
  }
  return f;
}

static void make_status(const struct svc *svc,char status[20])
{
  unsigned long u;

  taia_pack(status,&svc->when);

  u = (unsigned long) svc->pid;
  status[12] = u; u >>= 8;
  status[13] = u; u >>= 8;
  status[14] = u; u >>= 8;
  status[15] = u;

  status[16] = (svc->pid ? svc->flagpaused : 0);
  status[17] = (svc->flagwant ? (svc->flagwantup ? 'u' : 'd') : 0);
  status[18] = svc->flagstatus;
  status[19] = 0;
}

void announce(void)
{
  int r;
  int w;
  char status[40];

  make_status(&svcmain,status);
  if (logpipe[0] < 0)
    w = 20;
  else {
    make_status(&svclog,status + 20);
    w = 40;
  }

  if (seek_begin(fdstatus)) {
    strerr_warn2(WARNING,"unable to seek in status: ",&strerr_sys);
    return;
  }
  r = write(fdstatus,status,w);
  if (r < w) {
    strerr_warn2(WARNING,"unable to write status: partial write",0);
    return;
  }
}

static void notify(const struct svc *svc,const char *notice,int code,int oldpid)
{
  char pidnum[FMT_ULONG];
  char codenum[FMT_ULONG];
  const char *argv[] = {
    "./notify",
    svc == &svclog ? "log" : runscript+2,
    notice,pidnum,codenum,0
  };

  pidnum[fmt_uint(pidnum,oldpid)] = 0;
  codenum[fmt_uint(codenum,code)] = 0;
  forkexecve(argv,-1);
}

void pidchange(struct svc *svc,const char *notice,int code,int oldpid)
{
  taia_now(&svc->when);

  if (notice != 0 && stat_isexec("notify") > 0)
    notify(svc,notice,code,oldpid);
  announce();
}

void trystart(struct svc *svc)
{
  const char *argv[] = { 0,0 };
  int f;
  int fd;

  if (svc == &svclog) {
    argv[0] = "./log";
    svclog.flagstatus = svstatus_running;
    fd = 0;
  }
  else {
    if (firstrun && stat_isexec("start") == 0)
      firstrun = 0;
    argv[0] = runscript = firstrun ? "./start" : "./run";
    svcmain.flagstatus = firstrun ? svstatus_starting : svstatus_running;
    fd = 1;
  }
  if ((f = forkexecve(argv,fd)) < 0)
    return;
  svc->pid = f;
  svc->flagpaused = 0;
  pidchange(svc,"start",0,f);
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
  struct svc *svc;

  announce();

  for (;;) {
    if (flagexit && !svcmain.pid && !svclog.pid) return;

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
      if (r == svcmain.pid)
	svc = &svcmain;
      else if (r == svclog.pid)
	svc = &svclog;
      else
	continue;
      killpid = svc->pid;
      svc->pid = 0;
      if (!wait_crashed(wstat) && wait_exitcode(wstat) == 100) {
	svc->flagwantup = 0;
	svc->flagstatus = svstatus_failed;
      }
      else if (!svc->flagwant || !svc->flagwantup)
	svc->flagstatus = svstatus_stopped;
      pidchange(svc, wait_crashed(wstat) ? "killed" : "exit",
		wait_crashed(wstat) ? wait_stopsig(wstat) : wait_exitcode(wstat),
		killpid);
      firstrun = 0;
      if (flagexit) return;
      if (svc->flagwant && svc->flagwantup) trystart(svc);
      break;
    }

    svc = &svcmain;
    killpid = svc->pid;
    while (read(fdcontrol,&ch,1) == 1)
      switch(ch) {
        case '+':
	  killpid = -svc->pid;
	  break;
        case 'L':
	  svc = &svclog;
	  killpid = svc->pid;
	  break;
	case 'd':
	  svc->flagwant = 1;
	  svc->flagwantup = 0;
	  if (killpid) {
	    kill(killpid,SIGTERM);
	    kill(killpid,SIGCONT);
	    svc->flagpaused = 0;
	    svc->flagstatus = svstatus_stopping;
	  }
	  else
	    svc->flagstatus = svstatus_stopped;
	  announce();
	  break;
	case 'u':
	  if (svc == &svcmain)
	    firstrun = !svcmain.flagwantup;
	  svc->flagwant = 1;
	  svc->flagwantup = 1;
	  if (!svc->pid)
	    svc->flagstatus = svstatus_starting;
	  announce();
	  if (!svc->pid) trystart(svc);
	  break;
	case 'o':
	  svc->flagwant = 0;
	  announce();
	  if (!svc->pid) trystart(svc);
	  break;
	case 'a':
	  if (killpid) kill(killpid,SIGALRM);
	  break;
	case 'h':
	  if (killpid) kill(killpid,SIGHUP);
	  break;
	case 'k':
	  if (killpid) kill(killpid,SIGKILL);
	  break;
	case 't':
	  if (killpid) kill(killpid,SIGTERM);
	  break;
	case 'i':
	  if (killpid) kill(killpid,SIGINT);
	  break;
	case 'q':
	  if (killpid) kill(killpid,SIGQUIT);
	  break;
	case '1':
	  if (killpid) kill(killpid,SIGUSR1);
	  break;
	case '2':
	  if (killpid) kill(killpid,SIGUSR2);
	  break;
        case 'w':
	  if (killpid) kill(killpid,SIGWINCH);
	  break;
	case 'p':
	  svc->flagpaused = 1;
	  announce();
	  if (killpid) kill(killpid,SIGSTOP);
	  break;
	case 'c':
	  svc->flagpaused = 0;
	  announce();
	  if (killpid) kill(killpid,SIGCONT);
	  break;
	case 'x':
	  flagexit = 1;
	  announce();
	  break;
      }
  }
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

  if (stat_isexec("log") > 0) {
    if (pipe(logpipe) != 0)
      strerr_die4sys(111,FATAL,"unable to create pipe for ",dir,": ");
    svclog.flagwantup = 1;
  }
  if (stat("down",&st) != -1) {
    svcmain.flagwantup = 0;
    svclog.flagwantup = 0;
  }
  else
    if (errno != error_noent)
      strerr_die4sys(111,FATAL,"unable to stat ",dir,"/down: ");

  if ((fntemp = svpath_make("")) == 0) die_nomem();
  if (mkdir(fntemp,0700) != 0 && errno != error_exist)
    strerr_die4sys(111,FATAL,"unable to create ",fntemp,": ");
  if ((fntemp = svpath_make("/status")) == 0) die_nomem();
  fdstatus = open_trunc(fntemp);
  if (fdstatus == -1)
    strerr_die4sys(111,FATAL,"unable to open ",fntemp," for writing: ");
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

  pidchange(&svcmain,0,0,0);

  if ((fntemp = svpath_make("/ok")) == 0) die_nomem();
  fifo_make(fntemp,0600);
  fdok = open_read(fntemp);
  if (fdok == -1)
    strerr_die4sys(111,FATAL,"unable to read ",fntemp,": ");
  closeonexec(fdok);

  if (!svclog.flagwant || svclog.flagwantup) trystart(&svclog);
  if (!svcmain.flagwant || svcmain.flagwantup) trystart(&svcmain);

  doit();
  announce();
  _exit(0);
}

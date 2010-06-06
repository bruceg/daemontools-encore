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
#include "coe.h"
#include "ndelay.h"
#include "env.h"
#include "iopause.h"
#include "taia.h"
#include "deepsleep.h"
#include "stralloc.h"

#define FATAL "supervise: fatal: "
#define WARNING "supervise: warning: "

const char *dir;
stralloc svdir = {0};
stralloc fntemp = {0};
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

char status[18];

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

  if (access("start", X_OK) != 0) firstrun = 0;
  run[0] = firstrun ? "./start" : "./run";
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
	if (!wait_crashed(wstat) && wait_exitcode(wstat) == 100)
	  flagwantup = 0;
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
	  if (pid) { kill(killpid,SIGTERM); kill(killpid,SIGCONT); flagpaused = 0; }
	  announce();
	  break;
	case 'u':
	  firstrun = !flagwantup;
	  flagwant = 1;
	  flagwantup = 1;
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

void make_filename(stralloc *s, const char *suffix)
{
  if (!stralloc_copy(s,&svdir)
      || !stralloc_cats(s,suffix)
      || !stralloc_0(s))
    die_nomem();
}

void make_filenames(void)
{
  const char *base;
  base = env_get("SUPERVISEDIR");
  if (base == 0)
    base = "supervise";
  if (!stralloc_copys(&svdir,base))
    die_nomem();
  if (base[0] == '/') {
    char cwd[8192];
    char *ptr;
    if (getcwd(cwd,sizeof cwd) == 0)
      strerr_die2sys(111,FATAL,"unable to determine cwd");
    for (ptr = cwd+1; *ptr != 0; ++ptr)
      if (*ptr == '/')
	*ptr = ':';
    if (!stralloc_cats(&svdir, cwd))
      die_nomem();
  }
  make_filename(&fn_status,"/status");
  make_filename(&fn_status_new,"/status.new");
}

int main(int argc,char **argv)
{
  struct stat st;

  dir = argv[1];
  if (!dir || argv[2])
    strerr_die1x(100,"supervise: usage: supervise dir");

  if (pipe(selfpipe) == -1)
    strerr_die4sys(111,FATAL,"unable to create pipe for ",dir,": ");
  coe(selfpipe[0]);
  coe(selfpipe[1]);
  ndelay_on(selfpipe[0]);
  ndelay_on(selfpipe[1]);

  sig_block(sig_child);
  sig_catch(sig_child,trigger);

  if (chdir(dir) == -1)
    strerr_die4sys(111,FATAL,"unable to chdir to ",dir,": ");
  make_filenames();

  if (stat("down",&st) != -1)
    flagwantup = 0;
  else
    if (errno != error_noent)
      strerr_die4sys(111,FATAL,"unable to stat ",dir,"/down: ");

  make_filename(&fntemp,"");
  if (mkdir(fntemp.s,0700) != 0 && errno != error_exist)
    strerr_die4sys(111,FATAL,"unable to create ",fntemp.s,": ");
  make_filename(&fntemp,"/lock");
  fdlock = open_append(fntemp.s);
  if ((fdlock == -1) || (lock_exnb(fdlock) == -1))
    strerr_die4sys(111,FATAL,"unable to acquire ",fntemp.s,": ");
  coe(fdlock);

  make_filename(&fntemp,"/control");
  fifo_make(fntemp.s,0600);
  fdcontrol = open_read(fntemp.s);
  if (fdcontrol == -1)
    strerr_die4sys(111,FATAL,"unable to read ",fntemp.s,": ");
  coe(fdcontrol);
  ndelay_on(fdcontrol); /* shouldn't be necessary */
  fdcontrolwrite = open_write(fntemp.s);
  if (fdcontrolwrite == -1)
    strerr_die4sys(111,FATAL,"unable to write ",fntemp.s,": ");
  coe(fdcontrolwrite);

  pidchange();
  announce();

  make_filename(&fntemp,"/ok");
  fifo_make(fntemp.s,0600);
  fdok = open_read(fntemp.s);
  if (fdok == -1)
    strerr_die4sys(111,FATAL,"unable to read ",fntemp.s,": ");
  coe(fdok);

  if (!flagwant || flagwantup) trystart();

  doit();
  announce();
  _exit(0);
}

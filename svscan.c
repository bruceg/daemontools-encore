#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "direntry.h"
#include "strerr.h"
#include "error.h"
#include "open.h"
#include "wait.h"
#include "closeonexec.h"
#include "fd.h"
#include "env.h"
#include "str.h"
#include "byte.h"
#include "pathexec.h"
#include "conf_svscan_log.c"

#define SERVICES 1000

#define INFO "svscan: info: "
#define WARNING "svscan: warning: "
#define FATAL "svscan: fatal: "

struct {
  unsigned long dev;
  unsigned long ino;
  int flagactive;
  int flaglog;
  int pid; /* 0 if not running */
  int pidlog; /* 0 if not running */
  int pi[2]; /* defined if flaglog */
} x[SERVICES];
int numx = 0;
int logx = -1;
int logpipe[2];

char fnlog[260];

void start(const char *fn)
{
  unsigned int fnlen;
  struct stat st;
  int child;
  int i;
  const char *args[3];
  int islog;

  islog = !str_diff(fn,conf_svscan_log);
  if (fn[0] == '.' && !islog) return;

  if (stat(fn,&st) == -1) {
    strerr_warn4(WARNING,"unable to stat ",fn,": ",&strerr_sys);
    return;
  }

  if ((st.st_mode & S_IFMT) != S_IFDIR) return;

  for (i = 0;i < numx;++i)
    if (x[i].ino == st.st_ino)
      if (x[i].dev == st.st_dev)
	break;

  if (i == numx) {
    if (numx >= SERVICES) {
      strerr_warn4(WARNING,"unable to start ",fn,": running too many services",0);
      return;
    }
    x[i].ino = st.st_ino;
    x[i].dev = st.st_dev;
    x[i].pid = 0;
    x[i].pidlog = 0;
    x[i].flaglog = 0;

    fnlen = str_len(fn);
    if (fnlen <= 255) {
      byte_copy(fnlog,fnlen,fn);
      byte_copy(fnlog + fnlen,5,"/log");
      if (stat(fnlog,&st) == 0 && S_ISDIR(st.st_mode))
	x[i].flaglog = 1;
      else
	if (errno != error_noent) {
          strerr_warn4(WARNING,"unable to stat ",fn,"/log: ",&strerr_sys);
          return;
	}
    }

    if (x[i].flaglog) {
      if (pipe(x[i].pi) == -1) {
        strerr_warn4(WARNING,"unable to create pipe for ",fn,": ",&strerr_sys);
        return;
      }
      closeonexec(x[i].pi[0]);
      closeonexec(x[i].pi[1]);
    }
    ++numx;
  }

  x[i].flagactive = 1;

  if (!x[i].pid)
    switch(child = fork()) {
      case -1:
        strerr_warn4(WARNING,"unable to fork for ",fn,": ",&strerr_sys);
        return;
      case 0:
        if (x[i].flaglog)
	  if (fd_move(1,x[i].pi[1]) == -1)
            strerr_die4sys(111,WARNING,"unable to set up descriptors for ",fn,": ");
	if (i == logx)
	  if (fd_move(0,logpipe[0]) == -1)
	    strerr_die4sys(111,WARNING,"unable to set up descriptors for ",fn,": ");
        args[0] = "supervise";
        args[1] = fn;
        args[2] = 0;
	pathexec_run(*args,args,(const char*const*)environ);
        strerr_die4sys(111,WARNING,"unable to start supervise ",fn,": ");
      default:
	x[i].pid = child;
    }

  if (x[i].flaglog && !x[i].pidlog)
    switch(child = fork()) {
      case -1:
        strerr_warn4(WARNING,"unable to fork for ",fn,"/log: ",&strerr_sys);
        return;
      case 0:
        if (fd_move(0,x[i].pi[0]) == -1)
          strerr_die4sys(111,WARNING,"unable to set up descriptors for ",fn,"/log: ");
	if (chdir(fn) == -1)
          strerr_die4sys(111,WARNING,"unable to switch to ",fn,": ");
        args[0] = "supervise";
        args[1] = "log";
        args[2] = 0;
	pathexec_run(*args,args,(const char*const*)environ);
        strerr_die4sys(111,WARNING,"unable to start supervise ",fn,"/log: ");
      default:
	x[i].pidlog = child;
    }
}

void direrror(void)
{
  strerr_warn2(WARNING,"unable to read directory: ",&strerr_sys);
}

void doit(void)
{
  DIR *dir;
  direntry *d;
  int i;
  int r;
  int wstat;

  for (;;) {
    r = wait_nohang(&wstat);
    if (!r) break;
    if (r == -1) {
      if (errno == error_intr) continue; /* impossible */
      break;
    }

    for (i = 0;i < numx;++i) {
      if (x[i].pid == r) { x[i].pid = 0; break; }
      if (x[i].pidlog == r) { x[i].pidlog = 0; break; }
    }
  }

  for (i = 0;i < numx;++i)
    x[i].flagactive = 0;

  dir = opendir(".");
  if (!dir) {
    direrror();
    return;
  }
  for (;;) {
    errno = 0;
    d = readdir(dir);
    if (!d) break;
    start(d->d_name);
  }
  if (errno) {
    direrror();
    closedir(dir);
    return;
  }
  closedir(dir);

  i = 0;
  while (i < numx) {
    if (!x[i].flagactive && !x[i].pid && !x[i].pidlog) {
      if (x[i].flaglog) {
        close(x[i].pi[0]);
        close(x[i].pi[1]);
        x[i].flaglog = 0;
      }
      x[i] = x[--numx];
      continue;
    }
    ++i;
  }
}

static void start_log(void)
{
  struct stat st;

  /* Make sure the standard file descriptors are open before creating any pipes. */
  if (fstat(0,&st) != 0 && errno == EBADF)
    (void) open_read("/dev/null");
  if (fstat(1,&st) != 0 && errno == EBADF)
    (void) open_write("/dev/null");
  if (fstat(2,&st) != 0 && errno == EBADF)
    (void) open_write("/dev/null");

  if (stat(conf_svscan_log,&st) == 0 && S_ISDIR(st.st_mode)) {
    logx = numx;
    if (pipe(logpipe) == -1)
      strerr_die4sys(111,FATAL,"unable to create pipe for ",conf_svscan_log,": ");
    closeonexec(logpipe[0]);
    closeonexec(logpipe[1]);
    start(conf_svscan_log);
    if (numx > logx && x[logx].pid != 0) {
      (void) fd_copy(1,logpipe[1]);
      (void) fd_move(2,logpipe[1]);
      strerr_warn2(INFO,"starting",0);
    }
  }
}

int main(int argc,char **argv)
{
  if (argv[0] && argv[1])
    if (chdir(argv[1]) == -1)
      strerr_die4sys(111,FATAL,"unable to chdir to ",argv[1],": ");

  start_log();

  for (;;) {
    doit();
    sleep(5);
  }
}

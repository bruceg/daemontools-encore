#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "strerr.h"
#include "error.h"
#include "open.h"
#include "fmt.h"
#include "tai.h"
#include "buffer.h"
#include "sgetopt.h"
#include "svpath.h"
#include "svstatus.h"

#define FATAL "svstat: fatal: "
#define WARNING "svstat: warning: "

char bspace[1024];
buffer b = BUFFER_INIT(buffer_unixwrite,1,bspace,sizeof bspace);

char strnum[FMT_ULONG];

int check_log = -1;

static void die_nomem(void)
{
  strerr_die2sys(111,FATAL,"unable to allocate memory");
}

static void die_usage(void)
{
  strerr_die1x(100,"svstat: usage: svstat [-L] [-l] dir [dir ...]");
}

static void showstatus(const char status[19], int r, int normallyup)
{
  const char *x;
  struct tai when;
  struct tai now;
  unsigned long pid;
  unsigned char want;
  unsigned char paused;
  enum svstatus statusflag;

  pid = (unsigned char) status[15];
  pid <<= 8; pid += (unsigned char) status[14];
  pid <<= 8; pid += (unsigned char) status[13];
  pid <<= 8; pid += (unsigned char) status[12];

  paused = status[16];
  want = status[17];
  statusflag = status[18];

  tai_unpack(status,&when);
  tai_now(&now);
  if (tai_less(&now,&when)) when = now;
  tai_sub(&when,&now,&when);

  if (pid) {
    buffer_puts(&b,"up (pid ");
    buffer_put(&b,strnum,fmt_ulong(strnum,pid));
    buffer_puts(&b,") ");
  }
  else
    buffer_puts(&b,"down ");

  buffer_put(&b,strnum,fmt_ulong(strnum,tai_approx(&when)));
  buffer_puts(&b," seconds");

  if (pid && !normallyup)
    buffer_puts(&b,", normally down");
  if (!pid && normallyup)
    buffer_puts(&b,", normally up");
  if (pid && paused)
    buffer_puts(&b,", paused");
  if (!pid && (want == 'u'))
    buffer_puts(&b,", want up");
  if (pid && (want == 'd'))
    buffer_puts(&b,", want down");
  if (r > 18) {
    switch (statusflag) {
    case svstatus_stopped: x = ", stopped"; break;
    case svstatus_starting: x = ", starting"; break;
    case svstatus_started: x = ", started"; break;
    case svstatus_running: x = ", running"; break;
    case svstatus_stopping: x = ", stopping"; break;
    case svstatus_failed: x=", failed"; break;
    case svstatus_orphanage: x=", orphanage"; break;
    default: x = ", status unknown";
    }
    if (pid && (want == '\000'))
      buffer_puts(&b,", once");
    if (x)
      buffer_puts(&b,x);
  }
}

void doit(const char *dir)
{
  struct stat st;
  int r;
  int fd;
  const char *x;
  const char *fntemp;
  char status[40];
  unsigned char normallyup;

  buffer_puts(&b,dir);

  if (chdir(dir) == -1) {
    x = error_str(errno);
    buffer_puts(&b,": unable to chdir: ");
    buffer_puts(&b,x);
    return;
  }
  if (!svpath_init()) {
    strerr_warn4sys(WARNING,"unable to set up control path for ",dir,"");
    return;
  }

  normallyup = 0;
  if (stat("down",&st) == -1) {
    if (errno != error_noent) {
      x = error_str(errno);
      buffer_puts(&b,": unable to stat down: ");
      buffer_puts(&b,x);
      return;
    }
    normallyup = 1;
  }

  if ((fntemp = svpath_make("/ok")) == 0) die_nomem();
  fd = open_write(fntemp);
  if (fd == -1) {
    if (errno == error_nodevice) {
      buffer_puts(&b,": supervise not running");
      return;
    }
    x = error_str(errno);
    buffer_puts(&b,": unable to open ");
    buffer_puts(&b,fntemp);
    buffer_puts(&b,": ");
    buffer_puts(&b,x);
    return;
  }
  close(fd);

  if ((fntemp = svpath_make("/status")) == 0) die_nomem();
  fd = open_read(fntemp);
  if (fd == -1) {
    x = error_str(errno);
    buffer_puts(&b,": unable to open ");
    buffer_puts(&b,fntemp);
    buffer_puts(&b,": ");
    buffer_puts(&b,x);
    return;
  }
  r = buffer_unixread(fd,status,sizeof status);
  close(fd);
  if (r < 18) {
    if (r == -1)
      x = error_str(errno);
    else
      x = "bad format";
    buffer_puts(&b,": unable to read ");
    buffer_puts(&b,fntemp);
    buffer_puts(&b,": ");
    buffer_puts(&b,x);
    return;
  }
  if (check_log != 1) {
    buffer_puts(&b,": ");
    showstatus(status,r,normallyup);
  }
  if (check_log != 0) {
    if (r >= 20+18) {
      if (check_log < 0) {
	buffer_puts(&b,"\n");
	buffer_puts(&b,dir);
      }
      buffer_puts(&b," log: ");
      showstatus(status+20,r-20,normallyup);
    }
    else if (check_log == 1)
      buffer_puts(&b,": no log service");
  }
}

int main(int argc,const char *const *argv)
{
  int fdorigdir;
  const char *dir;
  int opt;

  while ((opt = getopt(argc,argv,"lL")) != opteof) {
    switch (opt) {
    case 'L':
      check_log = 1;
      break;
    case 'l':
      check_log = 0;
      break;
    default:
      die_usage();
    }
  }
  argv += optind;

  if (!argv[0])
    die_usage();

  fdorigdir = open_read(".");
  if (fdorigdir == -1)
    strerr_die2sys(111,FATAL,"unable to open current directory");

  while ((dir = *argv++) != 0) {
    doit(dir);
    buffer_puts(&b,"\n");
    if (fchdir(fdorigdir) == -1)
      strerr_die2sys(111,FATAL,"unable to set directory");
  }

  buffer_flush(&b);

  _exit(0);
}

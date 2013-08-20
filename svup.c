#include <unistd.h>
#include "strerr.h"
#include "error.h"
#include "open.h"
#include "sgetopt.h"
#include "svpath.h"
#include "svstatus.h"

#define FATAL "svup: fatal: "

static int checkstatus(const char status[19], int r)
{
  /* Check for a PID */
  return (status[12] || status[13] || status[14] || status[15]) /* Check for a PID */
    || (r > 18
	&& (status[18] == svstatus_started || status[18] == svstatus_running));
}

static void die_usage(void)
{
  strerr_die1x(100,"svup: usage: svup [-L] [-l] dir");
}

int main(int argc,const char *const *argv)
{
  int fd;
  const char *fn;
  char status[40];
  int rd;
  int opt;
  int check_log = -1;

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

  if (chdir(argv[0]) == -1)
    strerr_die3sys(111,FATAL,"unable to chdir to ",argv[1]);

  if (!svpath_init())
    strerr_die3sys(111,FATAL,"unable to setup control path for ",argv[1]);

  if ((fn = svpath_make("/ok")) == 0)
    strerr_die2sys(111,FATAL,"unable to allocate memory");
  fd = open_write(fn);
  if (fd == -1) {
    if (errno == error_noent) _exit(100);
    if (errno == error_nodevice) _exit(100);
    strerr_die3sys(111,FATAL,"unable to open ",fn);
  }
  close(fd);

  if ((fn = svpath_make("/status")) == 0)
    strerr_die2sys(111,FATAL,"unable to allocate memory");
  fd = open_read(fn);
  if (fd == -1) {
    if (errno == error_noent) _exit(100);
    strerr_die3sys(111,FATAL,"unable to open ",fn);
  }
  if ((rd = read(fd,status,sizeof status)) == -1)
    strerr_die3sys(111,FATAL,"unable to read ",fn);
  if (rd < 18)
    strerr_die4(111,FATAL,"bad data in ",fn,": truncated file",0);

  if (check_log <= 0)
    if (!checkstatus(status,rd))
      _exit(100);

  if (check_log != 0) {
    if (rd < 20+18) {
      if (check_log > 0)
	_exit(100);
    }
    else
      if (!checkstatus(status+20,rd-20))
	_exit(100);
  }
  _exit(0);
}

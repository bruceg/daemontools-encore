#include <unistd.h>
#include "strerr.h"
#include "error.h"
#include "open.h"
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

int main(int argc,char **argv)
{
  int fd;
  const char *fn;
  char status[40];
  int rd;

  if (!argv[1])
    strerr_die1x(100,"svup: usage: svup dir");

  if (chdir(argv[1]) == -1)
    strerr_die4sys(111,FATAL,"unable to chdir to ",argv[1],": ");

  if (!svpath_init())
    strerr_die4sys(111,FATAL,"unable to setup control path for ",argv[1],": ");

  if ((fn = svpath_make("/ok")) == 0)
    strerr_die2sys(111,FATAL,"unable to allocate memory");
  fd = open_write(fn);
  if (fd == -1) {
    if (errno == error_noent) _exit(100);
    if (errno == error_nodevice) _exit(100);
    strerr_die4sys(111,FATAL,"unable to open ",fn,": ");
  }
  close(fd);

  if ((fn = svpath_make("/status")) == 0)
    strerr_die2sys(111,FATAL,"unable to allocate memory");
  fd = open_read(fn);
  if (fd == -1) {
    if (errno == error_noent) _exit(100);
    strerr_die4sys(111,FATAL,"unable to open ",fn,": ");
  }
  if ((rd = read(fd,status,sizeof status)) == -1)
    strerr_die4sys(111,FATAL,"unable to read ",fn,": ");
  if (rd < 18)
    strerr_die4(111,FATAL,"bad data in ",fn,": truncated file",0);

  if (!checkstatus(status,rd))
    _exit(100);
  if (rd >= 20+18 && !checkstatus(status+20,rd-20))
    _exit(100);
  _exit(0);
}

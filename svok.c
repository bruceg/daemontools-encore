#include <unistd.h>
#include "strerr.h"
#include "error.h"
#include "open.h"
#include "svpath.h"

#define FATAL "svok: fatal: "

int main(int argc,char **argv)
{
  int fd;
  const char *fnok;

  if (!argv[1])
    strerr_die1x(100,"svok: usage: svok dir");

  if (chdir(argv[1]) == -1)
    strerr_die3sys(111,FATAL,"unable to chdir to ",argv[1]);

  if (!svpath_init())
    strerr_die3sys(111,FATAL,"unable to setup control path for ",argv[1]);
  if ((fnok = svpath_make("/ok")) == 0)
    strerr_die2sys(111,FATAL,"unable to allocate memory");
  fd = open_write(fnok);
  if (fd == -1) {
    if (errno == error_noent) _exit(100);
    if (errno == error_nodevice) _exit(100);
    strerr_die3sys(111,FATAL,"unable to open ",fnok);
  }

  _exit(0);
}

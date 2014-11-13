#include <sys/types.h>
#include <sys/time.h>
#include "fifo.h"
#include "open.h"
#include "strerr.h"
#include "error.h"
#include "substdio.h"
#include "readwrite.h"
#include "ndelay.h"

#define FATAL "fifo: fatal: "

char *fn;
int fd;
int fdwrite;

char outbuf[1024];
substdio ssout = SUBSTDIO_FDBUF(write,1,outbuf,sizeof outbuf);

char inbuf[1024];
substdio ssin;

int myread(fd,buf,len) int fd; char *buf; int len;
{
  if (substdio_flush(&ssout) == -1) return -1;
  return read(fd,buf,len);
}

void main(argc,argv)
int argc;
char **argv;
{
  fn = argv[1];
  if (!fn)
    strerr_die1x(100,"fifo: usage: fifo filename");

  if (fifo_make(fn,0600) == -1)
    if (errno != error_exist)
      strerr_die4sys(111,FATAL,"unable to create ",fn,": ");

  fd = open_read(fn);
  if (fd == -1)
    strerr_die4sys(111,FATAL,"unable to open ",fn," for reading: ");

  fdwrite = open_write(fn);
  if (fdwrite == -1)
    strerr_die4sys(111,FATAL,"unable to open ",fn," for writing: ");

  ndelay_off(fd);
  substdio_fdbuf(&ssin,myread,fd,inbuf,sizeof inbuf);

  switch (substdio_copy(&ssout,&ssin)) {
    case -2:
      strerr_die4sys(111,FATAL,"unable to read ",fn,": ");
    case -3:
      strerr_die2sys(111,FATAL,"unable to write output: ");
    case 0:
      strerr_die3x(111,FATAL,"end of file on ",fn);
  }
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "buffer.h"
#include "stralloc.h"
#include "getln.h"
#include "open.h"
#include "error.h"
#include "strerr.h"
#include "byte.h"
#include "scan.h"

stralloc target = {0};
char *to;

const char FATAL[] = "installer: fatal: ";
void nomem() { strerr_die2x(111,FATAL,"out of memory"); }

char inbuf[BUFFER_INSIZE];
char outbuf[BUFFER_OUTSIZE];
buffer bufin;
buffer bufout;

void doit(stralloc *line)
{
  char *x;
  unsigned int xlen;
  unsigned int i;
  unsigned long ul;
  char *type;
  char *uidstr;
  char *gidstr;
  char *modestr;
  char *mid;
  char *name;
  uid_t uid;
  gid_t gid;
  mode_t mode;
  int fdin;
  int fdout;
  int opt;

  x = line->s; xlen = line->len;
  opt = (*x == '?');
  x += opt;
  xlen -= opt;

  type = x;
  i = byte_chr(x,xlen,':'); if (i == xlen) return;
  x[i++] = 0; x += i; xlen -= i;

  uidstr = x;
  i = byte_chr(x,xlen,':'); if (i == xlen) return;
  x[i++] = 0; x += i; xlen -= i;

  gidstr = x;
  i = byte_chr(x,xlen,':'); if (i == xlen) return;
  x[i++] = 0; x += i; xlen -= i;

  modestr = x;
  i = byte_chr(x,xlen,':'); if (i == xlen) return;
  x[i++] = 0; x += i; xlen -= i;

  mid = x;
  i = byte_chr(x,xlen,':'); if (i == xlen) return;
  x[i++] = 0; x += i; xlen -= i;

  name = x;
  i = byte_chr(x,xlen,':'); if (i == xlen) return;
  x[i++] = 0; x += i; xlen -= i;

  if (!stralloc_copys(&target,to)) nomem();
  if (!stralloc_cats(&target,mid)) nomem();
  if (!stralloc_cats(&target,name)) nomem();
  if (!stralloc_0(&target)) nomem();
  if (xlen > 0) name = x;

  if (*uidstr) {
    scan_ulong(uidstr,&ul);
    uid = (uid_t)ul;
  }
  else
    uid = -1;
  if (*gidstr) {
    scan_ulong(gidstr,&ul);
    gid = (gid_t)ul;
  }
  else
    gid = -1;
  scan_8long(modestr,&ul);
  mode = ul;

  switch(*type) {
    case 'd':
      if (mkdir(target.s,0700) == -1)
        if (errno != error_exist)
	  strerr_die3sys(111,FATAL,"unable to mkdir ",target.s);
      break;

    case 'c':
      fdin = open_read(name);
      if (fdin == -1) {
	if (opt)
	  return;
	else
	  strerr_die3sys(111,FATAL,"unable to read ",name);
      }
      buffer_init(&bufin,buffer_unixread,fdin,inbuf,sizeof(inbuf));

      fdout = open_trunc(target.s);
      if (fdout == -1)
	strerr_die3sys(111,FATAL,"unable to write ",target.s);
      buffer_init(&bufout,buffer_unixwrite,fdout,outbuf,sizeof(outbuf));

      switch(buffer_copy(&bufout,&bufin)) {
	case -2:
	  strerr_die3sys(111,FATAL,"unable to read ",name);
	case -3:
	  strerr_die3sys(111,FATAL,"unable to write ",target.s);
      }

      close(fdin);
      if (buffer_flush(&bufout) == -1)
	strerr_die3sys(111,FATAL,"unable to write ",target.s);
      if (fsync(fdout) == -1)
	strerr_die3sys(111,FATAL,"unable to write ",target.s);
      close(fdout);
      break;

    default:
      return;
  }

  if (chown(target.s,uid,gid) == -1)
    strerr_die3sys(111,FATAL,"unable to chown ",target.s);
  if (chmod(target.s,mode) == -1)
    strerr_die3sys(111,FATAL,"unable to chmod ",target.s);
}

char buf[256];
buffer in = BUFFER_INIT(buffer_unixread,0,buf,sizeof(buf));
stralloc line = {0};

int main(int argc,char **argv)
{
  int match;

  umask(077);

  to = argv[1];
  if (!to) strerr_die2x(100,FATAL,"installer: usage: install dir");

  for (;;) {
    if (getln(&in,&line,&match,'\n') == -1)
      strerr_die2sys(111,FATAL,"unable to read input");
    if (line.len > 0)
      line.s[--line.len] = 0;
    doit(&line);
    if (!match)
      _exit(0);
  }
  (void)argc;
  return 0;
}

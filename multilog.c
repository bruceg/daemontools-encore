#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "direntry.h"
#include "alloc.h"
#include "stralloc.h"
#include "buffer.h"
#include "strerr.h"
#include "error.h"
#include "open.h"
#include "lock.h"
#include "scan.h"
#include "str.h"
#include "byte.h"
#include "seek.h"
#include "timestamp.h"
#include "wait.h"
#include "closeonexec.h"
#include "env.h"
#include "fd.h"
#include "sig.h"
#include "match.h"
#include "deepsleep.h"

#define MAXLINE 1000

extern int rename(const char *,const char *);

#define FATAL "multilog: fatal: "
#define WARNING "multilog: warning: "

void pause3(const char *s1,const char *s2,const char *s3)
{
  strerr_warn4sys(WARNING,s1,s2,s3);
  deepsleep(5);
}

void pause5(const char *s1,const char *s2,const char *s3,const char *s4,const char *s5)
{
  strerr_warn6sys(WARNING,s1,s2,s3,s4,s5);
  deepsleep(5);
}

int fdstartdir;

int *f;

void f_init(char **script)
{
  int i;
  int fd;

  for (i = 0;script[i];++i)
    ;

  f = (int *) alloc(i * sizeof(*f));
  if (!f) strerr_die2x(111,FATAL,"out of memory");

  for (i = 0;script[i];++i) {
    fd = -1;
    if (script[i][0] == '=') {
      if (fchdir(fdstartdir) == -1)
        strerr_die2sys(111,FATAL,"unable to switch to starting directory");
      fd = open_append(script[i] + 1);
      if (fd == -1)
        strerr_die3sys(111,FATAL,"unable to create ",script[i] + 1);
      close(fd);
      fd = open_write(script[i] + 1);
      if (fd == -1)
        strerr_die3sys(111,FATAL,"unable to write ",script[i] + 1);
      closeonexec(fd);
    }
    f[i] = fd;
  }
}

struct cyclog {
  char buf[512];
  buffer ss;
  int fdcurrent;
  unsigned long bytes;
  unsigned long num;
  unsigned long size;
  const char *processor;
  const char *code_finished;
  const char *dir;
  int fddir;
  int fdlock;
  int flagselected;
} *c;
int cnum;

stralloc fn = {0};

int filesfit(struct cyclog *d)
{
  DIR *dir;
  direntry *x;
  int count;
  int i;

  dir = opendir(".");
  if (!dir) return -1;

  fn.s[0] = '@';
  fn.s[1] = 'z';
  fn.s[2] = 0;

  count = 0;
  for (;;) {
    errno = 0;
    x = readdir(dir);
    if (!x) break;
    if (x->d_name[0] == '@')
      if (str_len(x->d_name) >= 25) {
        ++count;
        if (str_diff(x->d_name,fn.s) < 0) {
          for (i = 0;i < 25;++i)
            fn.s[i] = x->d_name[i];
          fn.s[25] = 0;
        }
      }
  }
  if (errno) { closedir(dir); return -1; }
  closedir(dir);

  if (count < d->num) return 1;

  dir = opendir(".");
  if (!dir) return -1;

  for (;;) {
    errno = 0;
    x = readdir(dir);
    if (!x) break;
    if (x->d_name[0] == '@')
      if (str_len(x->d_name) >= 25)
        if (str_start(x->d_name,fn.s)) {
	  unlink(x->d_name);
	  break;
        }
  }
  if (errno) { closedir(dir); return -1; }
  closedir(dir);
  return 0;
}

void finish(struct cyclog *d,const char *file,const char *code)
{
  struct stat st;
  int fnlen;

  for (;;) {
    if (stat(file,&st) == 0) break;
    if (errno == error_noent) return;
    pause5("unable to stat ",d->dir,"/",file,", pausing");
  }

  if (st.st_nlink == 1)
    for (;;) {
      fnlen = fmt_tai64nstamp(fn.s);
      fn.s[fnlen++] = '.';
      do {
	fn.s[fnlen++] = *code;
      } while (*code++ != 0);

      if (link(file,fn.s) == 0) break;
      pause5("unable to link to ",d->dir,"/",fn.s,", pausing");
    }

  while (unlink(file) == -1)
    pause5("unable to remove ",d->dir,"/",file,", pausing");

  for (;;)
    switch(filesfit(d)) {
      case 1: return;
      case -1: pause3("unable to read ",d->dir,", pausing");
    }
}

void startprocessor(struct cyclog *d)
{
  const char *args[4];
  int fd;

  sig_uncatch(sig_term);
  sig_uncatch(sig_alarm);
  sig_unblock(sig_term);
  sig_unblock(sig_alarm);

  fd = open_read("previous");
  if (fd == -1) return;
  if (fd_move(0,fd) == -1) return;
  fd = open_trunc("processed");
  if (fd == -1) return;
  if (fd_move(1,fd) == -1) return;
  fd = open_read("state");
  if (fd == -1) return;
  if (fd_move(4,fd) == -1) return;
  fd = open_trunc("newstate");
  if (fd == -1) return;
  if (fd_move(5,fd) == -1) return;

  args[0] = "sh";
  args[1] = "-c";
  args[2] = d->processor;
  args[3] = 0;
  execve("/bin/sh",(char*const*)args,environ);
}

void fullcurrent(struct cyclog *d)
{
  int fd;
  int pid;
  int wstat;

  while (fchdir(d->fddir) == -1)
    pause3("unable to switch to ",d->dir,", pausing");

  while (fsync(d->fdcurrent) == -1)
    pause3("unable to write ",d->dir,"/current to disk, pausing");
  close(d->fdcurrent);

  while (rename("current","previous") == -1)
    pause3("unable to rename current to previous in directory ",d->dir,", pausing");
  while ((d->fdcurrent = open_append("current")) == -1)
    pause3("unable to create ",d->dir,"/current, pausing");
  closeonexec(d->fdcurrent);
  d->bytes = 0;
  while (fchmod(d->fdcurrent,0644) == -1)
    pause3("unable to set mode of ",d->dir,"/current, pausing");

  while (chmod("previous",0744) == -1)
    pause3("unable to set mode of ",d->dir,"/previous, pausing");

  if (!d->processor)
    finish(d,"previous",d->code_finished);
  else {
    for (;;) {
      while ((pid = fork()) == -1)
        pause3("unable to fork for processor in ",d->dir,", pausing");
      if (!pid) {
        startprocessor(d);
        strerr_die3sys(111,FATAL,"unable to run ",d->processor);
      }
      if (wait_pid(&wstat,pid) == -1)
        pause3("wait failed for processor in ",d->dir,", pausing");
      else if (wait_crashed(wstat))
        pause3("processor crashed in ",d->dir,", pausing");
      else if (!wait_exitcode(wstat))
        break;
      strerr_warn4(WARNING,"processor failed in ",d->dir,", pausing",0);
      deepsleep(5);
    }

    while ((fd = open_append("processed")) == -1)
      pause3("unable to create ",d->dir,"/processed, pausing");
    while (fsync(fd) == -1)
      pause3("unable to write ",d->dir,"/processed to disk, pausing");
    while (fchmod(fd,0744) == -1)
      pause3("unable to set mode of ",d->dir,"/processed, pausing");
    close(fd);

    while ((fd = open_append("newstate")) == -1)
      pause3("unable to create ",d->dir,"/newstate, pausing");
    while (fsync(fd) == -1)
      pause3("unable to write ",d->dir,"/newstate to disk, pausing");
    close(fd);

    while (unlink("previous") == -1)
      pause3("unable to remove ",d->dir,"/previous, pausing");
    while (rename("newstate","state") == -1)
      pause3("unable to rename newstate to state in directory ",d->dir,", pausing");
    finish(d,"processed",d->code_finished);
  }
}

int c_write(int pos,char *buf,int len)
{
  struct cyclog *d;
  int w;

  d = c + pos;

  if (d->bytes >= d->size)
    fullcurrent(d);

  if (len >= d->size - d->bytes)
    len = d->size - d->bytes;

  if (d->bytes + len >= d->size - 2000) {
    w = byte_rchr(buf,len,'\n');
    if (w < len)
      len = w + 1;
  }

  for (;;) {
    w = write(d->fdcurrent,buf,len);
    if (w > 0) break;
    pause3("unable to write to ",d->dir,"/current, pausing");
  }

  d->bytes += w;
  if (d->bytes >= d->size - 2000)
    if (buf[w - 1] == '\n')
      fullcurrent(d);

  return w;
}

void restart(struct cyclog *d)
{
  struct stat st;
  int fd;
  int flagprocessed;

  if (fchdir(fdstartdir) == -1)
    strerr_die2sys(111,FATAL,"unable to switch to starting directory");

  mkdir(d->dir,0700);
  d->fddir = open_read(d->dir);
  if ((d->fddir == -1) || (fchdir(d->fddir) == -1))
    strerr_die3sys(111,FATAL,"unable to open directory ",d->dir);
  closeonexec(d->fddir);

  d->fdlock = open_append("lock");
  if ((d->fdlock == -1) || (lock_exnb(d->fdlock) == -1))
    strerr_die3sys(111,FATAL,"unable to lock directory ",d->dir);
  closeonexec(d->fdlock);

  if (stat("current",&st) == -1) {
    if (errno != error_noent)
      strerr_die4sys(111,FATAL,"unable to stat ",d->dir,"/current");
  }
  else
    if (st.st_mode & 0100) {
      fd = open_append("current");
      if (fd == -1)
        strerr_die4sys(111,FATAL,"unable to append to ",d->dir,"/current");
      if (fchmod(fd,0644) == -1)
        strerr_die4sys(111,FATAL,"unable to set mode of ",d->dir,"/current");
      closeonexec(fd);
      d->fdcurrent = fd;
      d->bytes = st.st_size;
      return;
    }

  unlink("state");
  unlink("newstate");

  flagprocessed = 0;
  if (stat("processed",&st) == -1) {
    if (errno != error_noent)
      strerr_die4sys(111,FATAL,"unable to stat ",d->dir,"/processed");
  }
  else if (st.st_mode & 0100)
    flagprocessed = 1;

  if (flagprocessed) {
    unlink("previous");
    finish(d,"processed",d->code_finished);
  }
  else {
    unlink("processed");
    finish(d,"previous","u");
  }

  finish(d,"current","u");

  fd = open_trunc("state");
  if (fd == -1)
    strerr_die4sys(111,FATAL,"unable to write to ",d->dir,"/state");
  close(fd);
  fd = open_append("current");
  if (fd == -1)
    strerr_die4sys(111,FATAL,"unable to write to ",d->dir,"/current");
  if (fchmod(fd,0644) == -1)
    strerr_die4sys(111,FATAL,"unable to set mode of ",d->dir,"/current");
  closeonexec(fd);
  d->fdcurrent = fd;
  d->bytes = 0;
}

void c_init(char **script)
{
  int i;
  struct cyclog *d;
  const char *processor;
  unsigned long num;
  unsigned long size;
  const char *code_finished = "s";

  cnum = 0;
  for (i = 0;script[i];++i)
    if ((script[i][0] == '.') || (script[i][0] == '/'))
      ++cnum;

  c = (struct cyclog *) alloc(cnum * sizeof(*c));
  if (!c) strerr_die2x(111,FATAL,"out of memory");

  d = c;
  processor = 0;
  num = 10;
  size = 99999;

  for (i = 0;script[i];++i)
    if (script[i][0] == 's') {
      scan_ulong(script[i] + 1,&size);
      if (size < 4096) size = 4096;
      if (size > 2147483647) size = 2147483647;
    }
    else if (script[i][0] == 'n') {
      scan_ulong(script[i] + 1,&num);
      if (num < 2) num = 2;
    }
    else if (script[i][0] == '!') {
      processor = script[i] + 1;
    }
    else if (script[i][0] == 'w') {
      code_finished = script[i] + 1;
      if (!stralloc_ready(&fn,str_len(code_finished)+TIMESTAMP+1))
	strerr_die2sys(111,FATAL,"unable to allocate memory");
    }
    else if ((script[i][0] == '.') || (script[i][0] == '/')) {
      d->num = num;
      d->size = size;
      d->processor = processor;
      d->code_finished = code_finished;
      d->dir = script[i];
      buffer_init(&d->ss,c_write,d - c,d->buf,sizeof d->buf);
      restart(d);
      ++d;
    }
}

void c_quit(void)
{
  int j;

  for (j = 0;j < cnum;++j) {
    buffer_flush(&c[j].ss);
    while (fsync(c[j].fdcurrent) == -1)
      pause3("unable to write ",c[j].dir,"/current to disk, pausing");
    while (fchmod(c[j].fdcurrent,0744) == -1)
      pause3("unable to set mode of ",c[j].dir,"/current, pausing");
  }
}

int flagexitasap = 0;
int flagforcerotate = 0;
int flagnewline = 1;

void exitasap(void)
{
  flagexitasap = 1;
}

void forcerotate(void)
{
  flagforcerotate = 1;
}

int flushread(int fd,char *buf,int len)
{
  int j;

  for (j = 0;j < cnum;++j)
    buffer_flush(&c[j].ss);
  if (flagforcerotate) {
    for (j = 0;j < cnum;++j)
      if (c[j].bytes > 0)
	fullcurrent(&c[j]);
    flagforcerotate = 0;
  }

  if (!len) return 0;
  if (flagexitasap) {
    if (flagnewline) return 0;
    len = 1;
  }

  sig_unblock(sig_term);
  sig_unblock(sig_alarm);

  len = read(fd,buf,len);

  sig_block(sig_term);
  sig_block(sig_alarm);

  if (len <= 0) return len;
  flagnewline = (buf[len - 1] == '\n');
  return len;
}

char inbuf[1024];
buffer ssin = BUFFER_INIT(flushread,0,inbuf,sizeof inbuf);

char line[MAXLINE+1];

void doit(char **script)
{
  int flageof;
  char ch;
  int j;
  int i;
  const char *action;
  int flagselected;
  int flagtimestamp;
  unsigned int linelen; /* 0 <= linelen <= MAXLINE */
  unsigned int oldlinelen = sizeof line;

  flagtimestamp = 0;
  if (script[0])
    if (script[0][0] == 't' || script[0][0] == 'T')
      flagtimestamp = script[0][0];

  for (i = 0;i <= MAXLINE;++i) line[i] = '\n';

  flageof = 0;
  for (;;) {
    linelen = 0;

    if (buffer_feed(&ssin) <= 0)
      return;
    if (flagtimestamp) {
      linelen = (flagtimestamp == 't')
	? fmt_tai64nstamp(line)
	: fmt_accustamp(line);
      line[linelen++] = ' ';
    }
    if (buffer_gets(&ssin,line+linelen,MAXLINE-linelen,'\n',&linelen) < 0)
      flageof = 1;

    for (i = linelen;i < oldlinelen;++i) line[i] = '\n';

    flagselected = 1;
    j = 0;
    for (i = 0;(action = script[i]) != 0;++i)
      switch(*action) {
        case 'F':
	  match = match_fnmatch;
	  break;
        case 'S':
	  match = match_simple;
	  break;
        case '+':
          if (!flagselected)
            if (match(action + 1,line,linelen))
              flagselected = 1;
          break;
        case '-':
          if (flagselected)
            if (match(action + 1,line,linelen))
              flagselected = 0;
          break;
        case 'e':
          if (flagselected) {
            if (linelen > 200) {
              buffer_put(buffer_2,line,200);
              buffer_puts(buffer_2,"...\n");
            }
            else {
              buffer_put(buffer_2,line,linelen);
              buffer_puts(buffer_2,"\n");
            }
            buffer_flush(buffer_2);
          }
          break;
        case '=':
          if (flagselected)
            for (;;) {
              while (seek_begin(f[i]) == -1)
                pause3("unable to move to beginning of ",action + 1,", pausing");
              if (write(f[i],line,MAXLINE+1) == MAXLINE+1)
                break;
              pause3("unable to write ",action + 1,", pausing");
            }
          break;
        case '.':
        case '/':
          c[j].flagselected = flagselected;
          ++j;
          break;
      }

    for (j = 0;j < cnum;++j)
      if (c[j].flagselected)
        buffer_put(&c[j].ss,line,linelen);
        
    while (linelen == MAXLINE) {
      linelen = 0;
      if (buffer_gets(&ssin,line,MAXLINE,'\n',&linelen) < 0) {
	flageof = 1;
	break;
      }
      if (linelen == 0)
	break;
      for (j = 0;j < cnum;++j)
	if (c[j].flagselected)
	  buffer_put(&c[j].ss,line,linelen);
    }

    for (j = 0;j < cnum;++j)
      if (c[j].flagselected) {
	ch = '\n';
        buffer_PUTC(&c[j].ss,ch);
      }

    if (flageof) return;

    oldlinelen = linelen;
  }
}

int main(int argc,char **argv)
{
  umask(022);

  if (!stralloc_ready(&fn,40))
    strerr_die2sys(111,FATAL,"unable to allocate memory");

  fdstartdir = open_read(".");
  if (fdstartdir == -1)
    strerr_die2sys(111,FATAL,"unable to switch to current directory");
  closeonexec(fdstartdir);

  sig_block(sig_term);
  sig_block(sig_alarm);
  sig_catch(sig_term,exitasap);
  sig_catch(sig_alarm,forcerotate);

  ++argv;
  f_init(argv);
  c_init(argv);
  doit(argv);
  c_quit();
  _exit(0);
}

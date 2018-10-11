/* Options:
    -d delay
        waits delay nanoseconds before exiting.
    -p fifo
        sends output through fifo instead of stdout. opens, writes, and
        closes fifo for each message sent. for signals that require
        delay (CONT and TERM), the fifo is only opened once. also does
        not exit after every signal unless a CONT/TERM pair.
    -w fifo
        opens fifo for write after signal handler set but before
        entering main loop and sends "ok\n".
    -x code
        exits with supplied code.
*/
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "byte.h"
#include "error.h"
#include "iopause.h"
#include "ndelay.h"
#include "scan.h"
#include "sig.h"
#include "str.h"
#include "stralloc.h"
#include "strerr.h"
#include "subgetopt.h"

#define WARNING "sleeper: warning: "
#define FATAL "sleeper: fatal: "
#define USAGE "sleeper: usage: sleeper "

static int selfpipe[2];
static stralloc opt_p = {0,0,0};
static stralloc opt_w = {0,0,0};
static long opt_d = -1;
static int opt_x = 0;

static void die_usage(void)
{
  strerr_die2x(100,USAGE,"[-d delay] [-p pfifo] [-w wfifo] [-x code]");
}

static void catch_sig(int sig)
{
  int ignored;
  char c;

  c = (char)sig;
  ignored = write(selfpipe[1],&c,1);
  (void)ignored;
}

static void show_sig(int sig, int fd)
{
  char buf[7+14+2] = "Caught ";
  const char *name;
  int ignored;
  int i;

  switch (sig) {
    case SIGALRM: name = "ALRM"; break;
    case SIGCONT: name = "CONT"; break;
    case SIGHUP: name = "HUP"; break;
    case SIGINT: name = "INT"; break;
    case SIGQUIT: name = "QUIT"; break;
    case SIGTERM: name = "TERM"; break;
    case SIGUSR1: name = "USR1"; break;
    case SIGUSR2: name = "USR2"; break;
    case SIGWINCH: name = "WINCH"; break;
    default: name = "unknown signal";
  }
  i = str_len(name);
  byte_copy(buf+7,i,name);
  i += 7;
  buf[i++] = '\n';
  ignored = write(fd,buf,i);
  (void)ignored;
}

static int openp(void)
{
  struct stat st;
  int fd;
  if (opt_p.len) {
    if (stat(opt_p.s,&st) == -1)
      strerr_die4sys(111,FATAL,"unable to stat ",opt_p.s,"");
    if ((st.st_mode & S_IFMT) != S_IFIFO)
      strerr_die3x(111,FATAL,"not a fifo: ",opt_p.s);
    while ((fd = open(opt_p.s,O_WRONLY|O_APPEND)) == -1) {
      if (errno == error_intr) continue;
      strerr_die4sys(111,FATAL,"open: ",opt_p.s,"");
    }
  }
  else fd = 1;
  return fd;
}

static void closep(int fd)
{
  if (opt_p.len) close(fd);
}

static void show_err(void)
{
  int ignored;

  ignored = write(1,"invalid signal\n",15);
  return;
  (void)ignored;
}

static void show_one(int sig)
{
  int fd;
  fd = openp();
  show_sig(sig,fd);
  closep(fd);
  return;
}

static void show_two(int sig1, int sig2)
{
  int fd;
  fd = openp();
  show_sig(sig1,fd);
  show_sig(sig2,fd);
  closep(fd);
  return;
}

int main(int argc, const char *const *argv)
{
  struct stat st;
  struct taia deadline;
  struct taia stamp;
  iopause_fd pausefd;
  unsigned long tmp;
  int r;
  int opt;
  int nc = 0;
  int nt = 0;
  int oc = 0;
  int ot = 0;
  int new_sig = 0;
  int old_sig = 0;
  int ignored;
  char buf;
  char p[2];

  if (pipe(selfpipe) == -1)
    strerr_die1sys(111,"sleeper: fatal: unable to create pipe");
  ndelay_on(selfpipe[1]);

  sig_catch(SIGALRM,catch_sig);
  sig_catch(SIGCONT,catch_sig);
  sig_catch(SIGHUP,catch_sig);
  sig_catch(SIGINT,catch_sig);
  sig_catch(SIGQUIT,catch_sig);
  sig_catch(SIGTERM,catch_sig);
  sig_catch(SIGUSR1,catch_sig);
  sig_catch(SIGUSR2,catch_sig);
  sig_catch(SIGWINCH,catch_sig);

  while ((opt = sgopt(argc,argv,"d:p:w:x:")) != sgoptdone)
    switch (opt) {
      case 'd':
        if (scan_ulong(sgoptarg,&tmp)) {
          if (tmp >= 0) {
            opt_d = (long)tmp;
            continue;
          }
        }
        strerr_warn2(WARNING,"-d invalid argument",0);
        die_usage();
      case 'p':
        stralloc_copys(&opt_p,sgoptarg);
        stralloc_0(&opt_p);
        continue;
      case 'w':
        stralloc_copys(&opt_w,sgoptarg);
        stralloc_0(&opt_w);
        continue;
      case 'x':
        if (scan_ulong(sgoptarg,&tmp)) {
          if (tmp >= 0) {
            opt_x = (int)tmp;
            continue;
          }
        }
        strerr_warn2(WARNING,"-x invalid argument",0);
        die_usage();
      default:
        p[0] = (char)sgoptproblem;
        p[1] = '\0';
        if (argv[sgoptind] && (sgoptind < argc))
          strerr_warn3(WARNING,"illegal option -",p,0);
        else
          strerr_warn4(WARNING,"-",p," missing argument",0);
        die_usage();
    }
  argv += sgoptind;

  if (opt_p.len) {
    if (stat(opt_p.s,&st) == -1)
      strerr_die4sys(111,FATAL,"unable to stat ",opt_p.s,"");
    if ((st.st_mode & S_IFMT) != S_IFIFO)
      strerr_die3x(111,FATAL,"not a fifo: ",opt_p.s);
  }

  if (opt_w.len) {
    int fifofd;
    if (stat(opt_w.s,&st) == -1)
      strerr_die4sys(111,FATAL,"unable to stat ",opt_w.s,"");
    if ((st.st_mode & S_IFMT) != S_IFIFO)
      strerr_die3x(111,FATAL,"not a fifo: ",opt_w.s);
    while ((fifofd = open(opt_w.s,O_WRONLY)) == -1) {
      if (errno == error_intr) continue;
      strerr_die4sys(111,FATAL,"open: ",opt_w.s,"");
    }
    ignored = write(fifofd,"ok\n",3);
    close(fifofd);
  }

  do {
    for (;;) {
      r = read(selfpipe[0],&buf,1);
      if (!r) break;
      if (r == -1) {
        if (errno == error_intr) continue;
        break;
      }

      new_sig = (int)buf;

      nc = new_sig == SIGCONT;
      nt = new_sig == SIGTERM;
      oc = old_sig == SIGCONT;
      ot = old_sig == SIGTERM;

      if (!nc && !nt && !oc && !ot) {show_one(new_sig);          break;   }
      if (!nc && !nt && !oc &&  ot) {show_two(SIGTERM, new_sig); break;   }
      if (!nc && !nt &&  oc && !ot) {show_two(SIGCONT, new_sig); break;   }
      if (!nc && !nt &&  oc &&  ot) {show_err();                 break;   }
      if (!nc &&  nt && !oc && !ot) {old_sig = SIGTERM;          continue;}
      if (!nc &&  nt && !oc &&  ot) {show_one(SIGTERM);          continue;}
      if (!nc &&  nt &&  oc && !ot) {show_two(SIGCONT, SIGTERM); break;   }
      if (!nc &&  nt &&  oc &&  ot) {show_err();                 break;   }
      if ( nc && !nt && !oc && !ot) {old_sig = SIGCONT;          continue;}
      if ( nc && !nt && !oc &&  ot) {show_two(SIGCONT, SIGTERM); break;   }
      if ( nc && !nt &&  oc && !ot) {show_one(SIGCONT);          continue;}
      if ( nc && !nt &&  oc &&  ot) {show_err();                 break;   }
      if ( nc &&  nt && !oc && !ot) {show_err();                 break;   }
      if ( nc &&  nt && !oc &&  ot) {show_err();                 break;   }
      if ( nc &&  nt &&  oc && !ot) {show_err();                 break;   }
      if ( nc &&  nt &&  oc &&  ot) {show_err();                 break;   }
    }
    old_sig = 0;
    if ((oc && nt) || (ot && nc)) break;
  } while (opt_p.len);

  if (opt_d >= 0) {
    deadline.sec.x = 0;
    deadline.nano = opt_d;
    deadline.atto = 0;
    while (deadline.nano > 999999999) {
      ++deadline.sec.x;
      deadline.nano -= 1000000000;
    }
    taia_now(&stamp);
    taia_add(&deadline,&deadline,&stamp);
    for (;;) {
      taia_now(&stamp);
      if (taia_less(&deadline,&stamp)) break;
      iopause(&pausefd,0,&deadline,&stamp);
    }
  }

  _exit(opt_x);
  (void)ignored;
}

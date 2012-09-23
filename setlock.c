#include <unistd.h>
#include "lock.h"
#include "open.h"
#include "str.h"
#include "error.h"
#include "strerr.h"
#include "pathexec.h"
#include "sgetopt.h"
#include "scan.h"
#include "fmt.h"
#include <time.h>
#include <signal.h>
#include "sig.h"
#include "stralloc.h"

#define INVALID "setlock: bad parameter value: "
#define FATAL "setlock: fatal: "
#define INFO "setlock: info: "

/* This sets the maximum delay to 60*60*24*7 seconds, or 1 week (far longer
   that should be necessary for any reasonable application of setlock).
   If a user specifies a longer time delay, then we flag an error and exit.
 */
#define MAX_DELAY 604800

int flagndelay = 0;
int flagx = 0;

/* This is the lock timeout, in seconds (from command line).
 */
unsigned long int  timeval = 0;

/* This is where in the filesystem to write PID,timestamp (also from the
   command line) in the following format:

   PID,timestamp

   NOTE: space for terminal \0 is included in FMT_ULONG, so space for the
   comma-separator comes for free in the allocation of pidcontent[] (see
   below).
 */
char *pidfile = 0;

/* seconds to block, after locking (from command line)
 */
unsigned long int  blockval = 0;

/* file to lock (from command line)
 */
const char *file;

void usage() {
  strerr_die1x(100,"setlock: usage: setlock [ -nNxX ] [ -p pidfile ] [ -t timeout ] [ -b blocktime ] file program [ arg ... ]");
}

/* The alarm() invocation (see below) causes this routine to catch a timeout
   signal after we've been waiting for 'timeval' seconds for a lock on 'file'.
   Note that the scope of "errno" is not local!
 */
void timed_out(void)
{
  char timestr[FMT_ULONG];

  if (flagx) _exit(0);
  fmt_uint(timestr,timeval);
  errno = error_timeout;
  strerr_die4sys(errno,INFO,"lock request timeout after ",timestr," seconds: ");
}

int main(int argc,const char *const *argv,const char *const *envp)
{
  char opt;
  int fd_lockfile, fd_pidfile;
  int idx;
  char timestr[FMT_ULONG];
  char pidcontent[2*FMT_ULONG];

  fmt_uint(timestr,MAX_DELAY);

  while ((opt = getopt(argc,argv,"nNxXp:t:b:")) != opteof) {
    switch(opt) {
      case 'n': flagndelay = 1; break;
      case 'N': flagndelay = 0; break;
      case 'x': flagx = 1; break;
      case 'X': flagx = 0; break;
      case 'p':
        /* Don't bother duping the optarg string: */
        pidfile = (char *)optarg;
        break;
      case 't':
        scan_ulong(optarg, &timeval);
        if (timeval > MAX_DELAY) {
          if (flagx) _exit(0);
          strerr_warn4(INVALID,"maximum timeout is ",timestr," seconds",NULL);
          usage();
        }
        else if (timeval <= 0) {
          if (flagx) _exit(0);
          strerr_warn2(INVALID,"timeout must be a positive integer",NULL);
          usage();
        }
        break;
      case 'b':
        scan_ulong(optarg, &blockval);
        if (blockval > MAX_DELAY) {
          if (flagx) _exit(0);
          strerr_warn4(INVALID,"maximum blocktime is ",timestr," seconds",NULL);
          usage();
        }
        else if (blockval <= 0) {
          if (flagx) _exit(0);
          strerr_warn2(INVALID,"blocktime must be a positive integer",NULL);
          usage();
        }
        break;
      default: usage();
    }
  }

  argv += optind;
  if (!*argv) usage();
  file = *argv++;
  if (!*argv) usage();

  fd_lockfile = open_append(file);
  if (fd_lockfile == -1) {
    if (flagx) _exit(0);
    strerr_die4sys(111,FATAL,"unable to open ",file,": ");
  }

  if (timeval) {
    sig_catch(SIGALRM,(void *)timed_out);
    alarm(timeval);
  }

  if ((flagndelay ? lock_exnb : lock_ex)(fd_lockfile) == -1) {
    if (flagx) _exit(0);
    strerr_die4sys(111,FATAL,"unable to lock ",file,": ");
  }

  if (pidfile != 0 && pidfile[0] != 0) {
    fd_pidfile = open_trunc(pidfile);
    if (fd_pidfile == -1) {
      if (flagx) _exit(0);
      strerr_die4sys(111,FATAL,"unable to write to ",pidfile,": ");
    }

    idx = fmt_uint(pidcontent,getpid());
    pidcontent[idx] = ',';
    fmt_uint(pidcontent+idx+1,(unsigned long int)time(NULL));

    write(fd_pidfile,pidcontent,str_len(pidcontent));
    close(fd_pidfile);
  }

  if (blockval) {
    sleep(blockval);
  }

  pathexec_run(*argv,argv,envp);
  strerr_die4sys(111,FATAL,"unable to run ",*argv,": ");
}

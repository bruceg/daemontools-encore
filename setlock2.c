#include <unistd.h>
#include "lock.h"
#include "open.h"
#include "strerr.h"
#include "string.h"
#include "pathexec.h"
#include "sgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>

/* This program is a modified version of the setlock program distributed
   with daemontools-0.76:

     setlock [ -nNxX ] lockfile program [ arg ... ]

   The command-line options for the original setlock program (documented
   at http://cr.yp.to/daemontools/setlock.html) are as follows:

   -n: No delay. If the lockfile is locked by another process then setlock
       immediately prints error message and exits with a nonzero status
       code.

   -N: (Default.) Delay. If the lockfile is locked by another process, setlock
       waits (indefinitely) until it can obtain a new lock.

   -x: If the lockfile cannot be opened (or created) or locked, setlock exits
       zero. 

   -X: (Default.) If the lockfile cannot be opened (or created) or locked,
       setlock prints an error message to STDERR and exits nonzero.

   This program differs from the original setlock in that it supports these
   additional options:

   -p pidfile:   Write PID,time to the indicated pidfile (where PID is the
                 process ID of the running process and time is the current
                 UTC time), after the lockfile is locked.

   -t timeout:   If the lockfile is locked by another process, wait up to
                 timeout seconds to obtain the lock before printing an error
                 message and exiting with a nonzero status code.

   -b blocksec:  After obtaining the lock (and writing the PID, if the -p
                 option is specified), sleep blocksec before running the
                 specified  program.

   -s:           Write error messages to syslog before writing them to STDERR
                 (also works for non-root users)

   NOTES:
   ------
   1) This wrapper program doesn't affect the UID or GID of the running
      process. To tweak these aspects of the process (eg to limit permissions)
      consider the as_user wrapper program or one of the standard wrappers
      in the daemontools suite (eg envuidgid). Note that wrapper invocations
      can be combined, but the order matters (particularly when changing
      UID or GID).

   2) The options supported by this wrapper program are parsed directly
      from the "front" (left end) of the command line and are *not* passed
      through to the specified program (whose options are specified towards
      the "back" (right end) of the command line), so there should be no
      danger of confusing arguments to the wrapper with arguments to the
      "wrapped" program. But since this wrapper is designed for use with
      the 'rpm' program, I've tried to minimize confusion by avoiding any
      option letters which are also used by rpm:

        http://www.netadmintools.com/html/8rpm.man.html

   MOTIVIATION:
   ------------
   I needed a workaround for this bug:

       https://bugzilla.redhat.com/show_bug.cgi?id=115152

   HISTORY:
   --------
   • 2012/08/06 added to daemontools-encore fork by pw@qualys.com
   • 2004/02/16 written by pw@qualys.com
*/

#define FATAL "setlock2: fatal: "
#define INFO "setlock2: "

/* This sets the maximum length of a PID string to be 17 digits, which seems
   safe (though arbitrary). If a real PID string is longer, then we'll write
   only the first 17 digits to the indicated pidfile.
 */
#define MAX_PID 18

/* This sets the maximum length of a UTC time string to be 17 digits, which
   is safe (though arbitrary). Since Feb 19, 2004 ~ 1077217000, ten decimal
   digits should take us from 1970 to about 2285 (315 years), and the datestamps
   produced by this program will start to be truncated in about 3 billion
   years.
 */
#define MAX_TIME 18

/* This sets the maximum delay to 60*60*24*7 seconds, or 1 week, which seems
   safe (though arbitrary). If a user specifies a longer time delay, then we
   flag an error and exit.
 */
#define MAX_DELAY 604800


void usage() {
  // there's no point sending the usage string to syslog...
  strerr_die1x(100,"setlock2: usage: setlock2 [ -nNxXp:t:b:s ] lockfile program [ arg ... ]");
}

int flagndelay = 0; // if nonzero, don't wait for lock (from command line)
int flagx = 0;      // if nonzero, don't print any errors (from command line)

/* The reverse() and itoa() routines defined below were borrowed
   from p.64 of K&R _The C Programming Language_ Edition 2.
 */

/* Reverse a string in place (used by itoa).
 */
void reverse(char s[]) {
  int c,i,j;
  for(i=0,j=strlen(s)-1; i<j; i++,j--) {
    c=s[i]; s[i]=s[j]; s[j]=c;
  }
} // reverse

/* Convert a signed integer into a string, whose allocated length is only
   guaranteed to be the passed limit (including the terminal NULL).
 */
void itoa(int n, char s[], int m) {
  int i, sign;
  // Since we need one slot for the terminal NULL character, and (potentially)
  // one slot for the leading minus sign, and at least one slot for a digit,
  // the passed limit must be at least 3.
  if (m < 3)
    strerr_die2sys(111,FATAL,"itoa called with m < 3 : ");
  m -= 2; // leave room for terminal null and sign char
  if ((sign = n) < 0) // record sign, and flip if necessary
    n = -n;
  i = 0;
  do {
    s[i++] = n%10 + '0';              // get next digit
  } while (((n/=10) > 0)&&(m-- > 0)); // remove it
  if (sign < 0)
    s[i++] = '-';
  s[i] = '\0';
  reverse(s);
} // itoa

char pidstr[MAX_PID]; // PID as printable string
int  pidval = 0;      // PID as comparable number
char *timestr = 0;    // lock timeout (in seconds) as string
int  timeval = 0;     // lock timeout (in seconds) as number
char *pidfile = 0;    // place to write PID and time (from command line)

char *blockstr = 0;   // seconds to block, as string (from command line)
int  blockval = 0;    // seconds to block, as number
int  flags = 0;       // write messages to syslog if on (from command line)

const char *file;     // lockfile (from command line)

/* The alarm() invocation (see below) causes this routine to catch a timeout
   signal after we've been waiting for 'timeval' seconds for a lock on 'file'.
   We can assume that the file and timestr strings are populated, since
   this is checked before alarm() is called.
 */
void timed_out(void)
{
  char msg[100];
  if (flags)
    syslog(LOG_INFO, "lock request for %s timed out after %s seconds", file, timestr);
  sprintf(msg, "lock request for %s timed out after %s seconds : ", file, timestr);
  strerr_die2sys(111, INFO, msg);
} // timed out

int main(int argc, const char *const *argv, const char *const *envp)
{
  int opt;               // for processing command line options
  int fd, fd2;           // file descriptors for lockfile and pidfile
  long t;                // current time as number (epoch sec)
  char tbuf[MAX_TIME+1]; // current time as string

  while ((opt = getopt(argc, argv, "nNxXp:t:b:s")) != opteof)
    switch(opt) {
      case 'n': flagndelay = 1; break;
      case 'N': flagndelay = 0; break;
      case 'x': flagx = 1; break;
      case 'X': flagx = 0; break;

      case 'p':
        pidfile = strdup(optarg);
        break;
      case 't':
        timestr = strdup(optarg);
        timeval = atoi(timestr);   // returns 0 for non-numerical string
        if (timeval > MAX_DELAY) {
          fprintf(stderr, "lock timeout (%d) > 1 week (%d) - too long!\n", timeval, MAX_DELAY);
          usage();
        }
        else if (timeval <= 0) {
          fprintf(stderr, "lock timeout (%d) must be a positive integer!\n", timeval);
          usage();
        }
        break;
      case 'b':
        blockstr = strdup(optarg);
        blockval = atoi(blockstr); // returns 0 for non-numerical string
        if (blockval > MAX_DELAY) {
          fprintf(stderr, "blocking time (%d) > 1 week (%d) - too long!\n", blockval, MAX_DELAY);
          usage();
        }
        else if (blockval <= 0) {
          fprintf(stderr, "blocking time (%d) must be a positive integer!\n", blockval);
          usage();
        }
        break;
      case 's': flags = 1; break;
      default: usage();
    }

  argv += optind;
  if (!*argv) {
    printf("Got no command-line parameters (need at least 2)\n");
    usage();
  }
  file = *argv++;
  if (!*argv) {
    printf("Got only one command-line parameter (need at least 2)\n");
    usage();
  }

  fd = open_append(file);
  if (fd == -1) {
    if (flagx)
      _exit(0);
    if (flags)
      syslog(LOG_ERR, "unable to open_append %s", file);
    strerr_die4sys(111, FATAL, "unable to open_append ", file, ": ");
  }

  if (timeval) { // legal timeout was specified
    signal(SIGALRM,timed_out); // set signal handler
    alarm(timeval);            // set alarm
  } // legal timeout was specified

  if ((flagndelay ? lock_exnb : lock_ex)(fd) == -1) {
    if (flagx)
      _exit(0);
    if (flags)
      syslog(LOG_ERR, "unable to lock %s", file);
    strerr_die4sys(111, FATAL, "unable to lock ", file, ": ");
  }

  if (timeval) // legal timeout was specified
    alarm(0);  // clear alarm

  if (pidfile && strlen(pidfile)) { // pidfile was specified
    fd2 = open_trunc(pidfile);
    if (fd2 == -1) {
      if (flagx)
        _exit(0);
      if (flags)
        syslog(LOG_ERR, "unable to open_trunc %s", pidfile);
      strerr_die4sys(111, FATAL, "unable to open_trunc ", pidfile, ": ");
    }
    pidval = getpid();
    itoa(pidval, pidstr, MAX_PID);
    write(fd2, pidstr, strlen(pidstr)); // don't write the null char!
    t = (long)time(NULL);
    tbuf[0] = ',';
    itoa(t, tbuf+1, MAX_TIME);
    write(fd2, tbuf, strlen(tbuf));     // don't write the null char!
    close(fd2);

  } // pidfile was specified

  if (blockval) { // blocking interval was specified  (and legal)
    sleep(blockval);
  } // blocking interval was specified (and legal)

  pathexec_run(*argv, argv, envp);
  if (flags)
    syslog(LOG_ERR, "unable to run %s", *argv);
  strerr_die4sys(111, FATAL, "unable to run ", *argv, ": ");
} // main

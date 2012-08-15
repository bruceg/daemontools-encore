#include <unistd.h>
#include "lock.h"
#include "open.h"
#include "string.h" // for strlen()
#include "stdlib.h" // for atoi()
#include "time.h"   // for time()
#include "stdio.h"  // for sprintf()
#include "strerr.h"
#include "pathexec.h"
#include "sgetopt.h"

#define FATAL "setlock: fatal: "
#define INFO "setlock: info: "

/* This sets the maximum length of a PID string to be 17 digits.
   If a real PID string is longer, then we'll write only the first 17 digits
   to the indicated pidfile.
 */
#define MAX_PID 18

/* This sets the maximum length of a UTC time string to be 17 digits.
   Since Feb 19, 2004 ~ 1077217000, ten decimal digits should take us from
   1970 to about 2285 (315 years), and datestamps produced by this program
   will start to be truncated in about 3 billion years.
 */
#define MAX_TIME 18

/* This sets the maximum delay to 60*60*24*7 seconds, or 1 week.
   If a user specifies a longer time delay, then we flag an error and exit.
 */
#define MAX_DELAY 604800

void usage() {
  strerr_die1x(100,"setlock: usage: setlock [ -nNxXptb ] file program [ arg ... ]");
}

int flagndelay = 0;
int flagx = 0;

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

const char *file;     // lockfile (from command line)

/* The alarm() invocation (see below) causes this routine to catch a timeout
   signal after we've been waiting for 'timeval' seconds for a lock on 'file'.
   We can assume that the file and timestr strings are populated, since
   this is checked before alarm() is called.
 */
void timed_out(void)
{
  char msg[100];
  sprintf(msg, "lock request for %s timed out after %s seconds : ",file,timestr);
  strerr_die2sys(111,INFO,msg);
} // timed out

int main(int argc,const char *const *argv,const char *const *envp)
{
  int opt;
  int fd, fd2;           // file descriptors for lockfile and pidfile
  long t;                // current time as number (epoch sec)
  char tbuf[MAX_TIME+1]; // current time as string

  while ((opt = getopt(argc,argv,"nNxX")) != opteof)
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
          fprintf(stderr, "lock timeout (%d sec) > maximum (%d sec)\n", timeval, MAX_DELAY);
          usage();
        }
        else if (timeval <= 0) {
          fprintf(stderr, "lock timeout (%d) in seconds must be a positive integer!\n", timeval);
          usage();
        }
        break;
      case 'b':
        blockstr = strdup(optarg);
        blockval = atoi(blockstr); // returns 0 for non-numerical string
        if (blockval > MAX_DELAY) {
          fprintf(stderr, "blocking time (%d sec) > maximum (%d sec)\n", blockval, MAX_DELAY);
          usage();
        }
        else if (blockval <= 0) {
          fprintf(stderr, "blocking time (%d) in seconds must be a positive integer!\n", blockval);
          usage();
        }
        break;
      default: usage();
    }

  argv += optind;
  if (!*argv) usage();
  file = *argv++;
  if (!*argv) usage();

  fd = open_append(file);
  if (fd == -1) {
    if (flagx) _exit(0);
    strerr_die4sys(111,FATAL,"unable to open ",file,": ");
  }

  if (timeval) { // legal timeout was specified
    signal(SIGALRM,(void *)timed_out); // set signal handler
    alarm(timeval);                    // set alarm
  } // legal timeout was specified

  if ((flagndelay ? lock_exnb : lock_ex)(fd) == -1) {
    if (flagx) _exit(0);
    strerr_die4sys(111,FATAL,"unable to lock ",file,": ");
  }

  if (timeval) // legal timeout was specified
    alarm(0);  // clear alarm

  if (pidfile && strlen(pidfile)) { // pidfile was specified
    fd2 = open_trunc(pidfile);
    if (fd2 == -1) {
      if (flagx) _exit(0);
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

  pathexec_run(*argv,argv,envp);
  strerr_die4sys(111,FATAL,"unable to run ",*argv,": ");
}

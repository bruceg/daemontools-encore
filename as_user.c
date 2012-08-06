#include <syslog.h>

#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

/* This program is a replacement for the setuidgid utility program distributed
   with daemontools-0.76, which runs a specified program with the UID and GID
   of a specified user account:

     setuidgid account program [ arg ... ]

   This program differs from the original setuidgid program (details of
   which are documented at http://cr.yp.to/daemontools/setuidgid.html) in
   that the auxilliary groups for the indicated account are added to the
   group list for the running process, and the following command-line options
   are supported:

   -s:   Write error messages to syslog before writing them to STDERR (should
         also work for non-root accounts). By default nothing is written
         to syslog.

   -x:   If the UID, GID, and groups cannot be set or the indicated program
         cannot be run, exit zero without printing anything to STDOUT/STDERR.
         Default behavior is to print any error messages to STDERR and exit
         nonzero.

   MOTIVATION:
   -----------
   I wrote a daemon that needed access to a collection of system resources,
   each of which had an associated user group for controlling access. So
   the daemon had to run as a particular user, with associated group and
   a set of auxilliary groups.

   HISTORY:
   --------
   • 2012/08/06 added to daemontools-encore fork by pw@qualys.com
   • 2004/02/16 written by pw@qualys.com
*/

#define FATAL "as_user: fatal: "

void usage() {
  // there's no point sending the usage string to syslog...
  strerr_die1x(100,"as_user: usage: as_user [ -xs ] account program [ arg ... ]");
}

int main(int argc, const char *const *argv, const char *const *envp)
{
  int opt;       // for processing command line options
  int flagx = 0; // if nonzero, print no errors
  int flags = 0; // if nonzero, write messages to syslog

  char msg[100];       // for preparing informative output
  int rc, errno;       // for catching useful return value information
  struct passwd *pw;   // password entry for user
  const char *account; // user account (from command line)

  while ((opt = getopt(argc,argv,"xs")) != opteof)
    switch(opt) {
      case 'x': flagx = 1; break;
      case 's': flags = 1; break;
      default: usage();
    }

  argv += optind;
  if (!*argv) {
    printf("Got no command-line parameters (need at least 2)\n");
    usage();
  }
  account = *argv++;
  if (!*argv) {
    printf("Got only one command-line parameter (need at least 2)\n");
    usage();
  }

  if ((pw = getpwnam(account)) == NULL) { // error getting pwent

    if (flagx)
      _exit(0);
    sprintf(msg, "getpwnam: Error getting record for account %s (errno %d)", account, errno);
    if (flags)
      syslog(LOG_ERR, msg);
    strerr_die3sys(111, FATAL, msg, ": ");

  } // error getting pwent

  // At this point pw contains our password entry for the specified account.
  // Note that group IDs are modified first, since after the UID is modified
  // all further rootly activity is blocked.

  // set auxilliary groups (account ~ pw->pw_name)
  rc = initgroups(account, pw->pw_gid);

  if (rc) {
    if (flagx)
      _exit(0);
    sprintf(msg, "initgroups: Error setting groups to match account %s (exit %d, errno %d)", account, rc, errno);
    if (flags)
      syslog(LOG_ERR, msg);
    strerr_die3sys(111, FATAL, msg, ": ");
  }

  rc = setregid(pw->pw_gid, pw->pw_gid); // set real & effective group ids
  if (rc) {
    if (flagx)
      _exit(0);
    sprintf(msg, "setregid: Error setting GID to match account %s (exit %d, errno %d)", account, rc, errno);
    if (flags)
      syslog(LOG_ERR, msg);
    strerr_die3sys(111, FATAL, msg, ": ");
  }

  rc = setreuid(pw->pw_uid, pw->pw_uid); // set real & effective user ids
  if (rc) {
    if (flagx)
      _exit(0);
    sprintf(msg, "setreuid: Error setting UID to match account %s (exit %d, errno %d)", account, rc, errno);
    if (flags)
      syslog(LOG_ERR, msg);
    strerr_die3sys(111, FATAL, msg, ": ");
  }

  pathexec_run(*argv, argv, envp);
  if (flagx)
    _exit(0);
  if (flags)
    syslog(LOG_ERR, "pathexec_run: Error running %s as account %s (errno %d)", *argv, account, errno);
  strerr_die4sys(111, FATAL, "unable to run ", *argv, ": ");
  _exit(1); // never get here
} // main

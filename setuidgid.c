#include <sys/types.h>
#include <pwd.h>
#include "prot.h"
#include "strerr.h"
#include "pathexec.h"
#include "sgetopt.h"

#define FATAL "setuidgid: fatal: "

void usage() {
  strerr_die1x(100,"setuidgid: usage: setuidgid [-s] account child");
}

const char *account;
struct passwd *pw;
int flag2ndgids = 0;

int main(int argc,const char *const *argv,const char *const *envp)
{
  int opt;

  while ((opt = getopt(argc,argv,"s")) != opteof)
    switch(opt) {
      case 's': flag2ndgids = 1; break;
      default: usage();
    }

  argv += optind;
  if (!*argv) usage();
  account = *argv++;
  if (!*argv) usage();

  pw = getpwnam(account);
  if (!pw)
    strerr_die3x(111,FATAL,"unknown account ",account);

  if (prot_gid(pw->pw_gid) == -1)
    strerr_die2sys(111,FATAL,"unable to setgid");
  if (flag2ndgids && prot_gids(pw->pw_name, pw->pw_gid) == -1)
    strerr_die2sys(111,FATAL,"unable to initgroups");
  if (prot_uid(pw->pw_uid) == -1)
    strerr_die2sys(111,FATAL,"unable to setuid");

  pathexec_run(*argv,argv,envp);
  strerr_die3sys(111,FATAL,"unable to run ",*argv);
}

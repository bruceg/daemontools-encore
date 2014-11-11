#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include "fmt.h"
#include "pathexec.h"
#include "prot.h"
#include "sgetopt.h"
#include "strerr.h"

#define FATAL "setuser: fatal: "

static void usage()
{
  strerr_die1x(100,"setuser: usage: setuser account child");
}

static void nomem()
{
  strerr_die2x(111,FATAL,"out of memory");
}


int main(int argc,const char *const *argv,const char *const *envp)
{
  struct passwd *pw;
  const char *account;
  char strnum[FMT_ULONG];

  account = *++argv;
  if (!account || !*++argv)
    usage();

  pw = getpwnam(account);
  if (!pw)
    strerr_die3x(111,FATAL,"unknown account ",account);

  if (!pathexec_env("HOME",pw->pw_dir)) nomem();
  if (!pathexec_env("SHELL",pw->pw_shell)) nomem();
  if (!pathexec_env("USER",pw->pw_name)) nomem();
  strnum[fmt_ulong(strnum,pw->pw_gid)] = 0;
  if (!pathexec_env("GID",strnum)) nomem();
  strnum[fmt_ulong(strnum,pw->pw_uid)] = 0;
  if (!pathexec_env("UID",strnum)) nomem();

  if (chdir(pw->pw_dir) != 0)
    strerr_die3sys(111,FATAL,"unable to chdir to ", pw->pw_dir);
  if (prot_gid(pw->pw_gid) == -1)
    strerr_die2sys(111,FATAL,"unable to setgid");
  if (prot_gids(pw->pw_name, pw->pw_gid) == -1)
    strerr_die2sys(111,FATAL,"unable to initgroups");
  if (prot_uid(pw->pw_uid) == -1)
    strerr_die2sys(111,FATAL,"unable to setuid");

  pathexec_run(*argv,argv,envp);
  strerr_die3sys(111,FATAL,"unable to run ",*argv);
}

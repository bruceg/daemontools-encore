#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>
#include "prot.h"
#include "strerr.h"
#include "pathexec.h"
#include "sgetopt.h"

#ifndef PAM_DATA_SILENT
#  define PAM_DATA_SILENT 0
#endif

#ifndef SETUIDGID_PAM_SERVICE_NAME_ENV
#  define SETUIDGID_PAM_SERVICE_NAME_ENV "SETUIDGID_PAM_SERVICE"
#endif
#ifndef SETUIDGID_PAM_SERVICE_NAME_DEFAULT
#  define SETUIDGID_PAM_SERVICE_NAME_DEFAULT "other"
#endif

#define FATAL "setuidgid: fatal: "

void usage() {
  strerr_die1x(100,"setuidgid: usage: setuidgid [-s] account child");
}

const char *account;
struct passwd *pw;
int flag2ndgids = 0;

static void pam_prep_user(void)
{
  pam_handle_t *pamh;
  const char *pam_service_name = getenv(SETUIDGID_PAM_SERVICE_NAME_ENV);
  struct pam_conv pam_conv;

  if (!pam_service_name)
    pam_service_name = SETUIDGID_PAM_SERVICE_NAME_DEFAULT;

  /* FIXME: Is this OK? */
  memset(&pam_conv, 0, sizeof(pam_conv));

  if (pam_start(pam_service_name, pw->pw_name, &pam_conv, &pamh) != PAM_SUCCESS)
    strerr_die2x(111,FATAL,"unable to initialize PAM");

  pam_set_item(pamh, PAM_USER, pw->pw_name);
  pam_setcred(pamh, PAM_ESTABLISH_CRED);

  if (pam_end(pamh, PAM_SUCCESS | PAM_DATA_SILENT) != PAM_SUCCESS)
    strerr_die2x(111,FATAL,"unable to finish PAM");
}

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

  pam_prep_user();

  if (prot_gid(pw->pw_gid) == -1)
    strerr_die2sys(111,FATAL,"unable to setgid");
  if (flag2ndgids && prot_gids(pw->pw_name, pw->pw_gid) == -1)
    strerr_die2sys(111,FATAL,"unable to initgroups");
  if (prot_uid(pw->pw_uid) == -1)
    strerr_die2sys(111,FATAL,"unable to setuid");

  pathexec_run(*argv,argv,envp);
  strerr_die3sys(111,FATAL,"unable to run ",*argv);
}

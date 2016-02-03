#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "env.h"
#include "pathexec.h"
#include "sgetopt.h"
#include "strerr.h"
#include "scan.h"
#include "str.h"

#define FATAL "softlimit: fatal: "

static void die_usage(void)
{
  strerr_die1x(100,"softlimit: usage: softlimit [-a allbytes] [-c corebytes] [-d databytes] [-f filebytes] [-l lockbytes] [-m membytes] [-o openfiles] [-p processes] [-r residentbytes] [-s stackbytes] [-t cpusecs] child");
}

static void doit(int resource,const char *arg)
{
  unsigned long u;
  struct rlimit r;

  if (getrlimit(resource,&r) == -1)
    strerr_die2sys(111,FATAL,"getrlimit failed");

  if (str_equal(arg,"="))
    r.rlim_cur = r.rlim_max;
  else if (str_equal(arg,"-"))
    r.rlim_cur = r.rlim_max = RLIM_INFINITY;
  else {
    if (arg[scan_ulong(arg,&u)]) die_usage();
    r.rlim_cur = u;
    if (r.rlim_cur > r.rlim_max)
      r.rlim_cur = r.rlim_max;
  }

  if (setrlimit(resource,&r) == -1)
    strerr_die2sys(111,FATAL,"setrlimit failed");
}

static void doenv(int resource,const char *name)
{
  const char *arg;
  if ((arg = env_get(name)) != 0)
    doit(resource,arg);
}

static void doallenv(void)
{
#ifdef RLIMIT_AS
  doenv(RLIMIT_AS,"SOFTLIMIT_ALLBYTES");
  doenv(RLIMIT_AS,"SOFTLIMIT_MEMBYTES");
#endif
#ifdef RLIMIT_VMEM
  doenv(RLIMIT_VMEM,"SOFTLIMIT_ALLBYTES");
  doenv(RLIMIT_VMEM,"SOFTLIMIT_MEMBYTES");
#endif
#ifdef RLIMIT_CORE
  doenv(RLIMIT_CORE,"SOFTLIMIT_COREBYTES");
#endif
#ifdef RLIMIT_DATA
  doenv(RLIMIT_DATA,"SOFTLIMIT_DATABYTES");
  doenv(RLIMIT_DATA,"SOFTLIMIT_MEMBYTES");
#endif
#ifdef RLIMIT_FSIZE
  doenv(RLIMIT_FSIZE,"SOFTLIMIT_FILEBYTES");
#endif
#ifdef RLIMIT_MEMLOCK
  doenv(RLIMIT_MEMLOCK,"SOFTLIMIT_LOCKEDBYTES");
  doenv(RLIMIT_MEMLOCK,"SOFTLIMIT_MEMBYTES");
#endif
#ifdef RLIMIT_STACK
  doenv(RLIMIT_STACK,"SOFTLIMIT_STACKBYTES");
  doenv(RLIMIT_STACK,"SOFTLIMIT_MEMBYTES");
#endif
#ifdef RLIMIT_NOFILE
  doenv(RLIMIT_NOFILE,"SOFTLIMIT_OPENFILES");
#endif
#ifdef RLIMIT_OFILE
  doenv(RLIMIT_OFILE,"SOFTLIMIT_OPENFILES");
#endif
#ifdef RLIMIT_NPROC
  doenv(RLIMIT_NPROC,"SOFTLIMIT_PROCS");
#endif
#ifdef RLIMIT_RSS
  doenv(RLIMIT_RSS,"SOFTLIMIT_RSSBYTES");
#endif
#ifdef RLIMIT_CPU
  doenv(RLIMIT_CPU,"SOFTLIMIT_CPUSECS");
#endif
}

int main(int argc,const char *const *argv,const char *const *envp)
{
  int opt;

  doallenv();
  while ((opt = getopt(argc,argv,"a:c:d:f:l:m:o:p:r:s:t:")) != opteof)
    switch(opt) {
      case '?':
	die_usage();
      case 'a':
#ifdef RLIMIT_AS
        doit(RLIMIT_AS,optarg);
#endif
#ifdef RLIMIT_VMEM
        doit(RLIMIT_VMEM,optarg);
#endif
        break;
      case 'c':
#ifdef RLIMIT_CORE
        doit(RLIMIT_CORE,optarg);
#endif
        break;
      case 'd':
#ifdef RLIMIT_DATA
        doit(RLIMIT_DATA,optarg);
#endif
        break;
      case 'f':
#ifdef RLIMIT_FSIZE
        doit(RLIMIT_FSIZE,optarg);
#endif
        break;
      case 'l':
#ifdef RLIMIT_MEMLOCK
        doit(RLIMIT_MEMLOCK,optarg);
#endif
        break;
      case 'm':
#ifdef RLIMIT_DATA
        doit(RLIMIT_DATA,optarg);
#endif
#ifdef RLIMIT_STACK
        doit(RLIMIT_STACK,optarg);
#endif
#ifdef RLIMIT_MEMLOCK
        doit(RLIMIT_MEMLOCK,optarg);
#endif
#ifdef RLIMIT_VMEM
        doit(RLIMIT_VMEM,optarg);
#endif
#ifdef RLIMIT_AS
        doit(RLIMIT_AS,optarg);
#endif
	break;
      case 'o':
#ifdef RLIMIT_NOFILE
        doit(RLIMIT_NOFILE,optarg);
#endif
#ifdef RLIMIT_OFILE
        doit(RLIMIT_OFILE,optarg);
#endif
        break;
      case 'p':
#ifdef RLIMIT_NPROC
        doit(RLIMIT_NPROC,optarg);
#endif
        break;
      case 'r':
#ifdef RLIMIT_RSS
        doit(RLIMIT_RSS,optarg);
#endif
        break;
      case 's':
#ifdef RLIMIT_STACK
        doit(RLIMIT_STACK,optarg);
#endif
        break;
      case 't':
#ifdef RLIMIT_CPU
        doit(RLIMIT_CPU,optarg);
#endif
        break;
    }

  argv += optind;
  if (!*argv) die_usage();

  pathexec_run(*argv,argv,envp);
  strerr_die3sys(111,FATAL,"unable to run ",*argv);
}

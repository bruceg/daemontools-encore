#include "strerr.h"

#ifdef __linux__
#include <sys/prctl.h>

#ifdef PR_SET_CHILD_SUBREAPER
#define HAS_SUBREAPER 1

extern int set_subreaper(void)
{
    return prctl(PR_SET_CHILD_SUBREAPER,1,0,0,0);
}

#endif
#endif

#ifdef __FreeBSD__
#include <sys/procctl.h>
#ifdef PROC_REAP_ACQUIRE
#define HAS_SUBREAPER 1

extern int set_subreaper(void)
{
    return procctl(P_PID,0,PROC_REAP_ACQUIRE,NULL);
}
#endif
#endif

#ifndef HAS_SUBREAPER
extern int set_subreaper(void)
{
    return -1;
}
#endif

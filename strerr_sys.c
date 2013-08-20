/* Public domain. */

#include "error.h"
#include "strerr.h"

struct strerr strerr_sys;

void strerr_sysinit(void)
{
  strerr_sys.who = 0;
  strerr_sys.x = ": ";
  strerr_sys.y = error_str(errno);
  strerr_sys.z = "";
}

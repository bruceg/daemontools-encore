/* Public domain. */

#include <fcntl.h>
#include "closeonexec.h"

int closeonexec(int fd)
{
  return fcntl(fd,F_SETFD,1);
}

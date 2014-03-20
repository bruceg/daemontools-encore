/* Public domain. */

#include <string.h>
#include "byte.h"

unsigned int byte_chr(const char *s,unsigned int n,int c)
{
  const char *t = memchr(s,c,n);
  return (t == NULL) ? n : t - s;
}

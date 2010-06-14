/* Public domain. */

#include "scan.h"

unsigned int scan_ulong(const char *s,unsigned long *u)
{
  unsigned int pos = 0;
  unsigned long result = 0;
  unsigned long c;
  while ((c = (unsigned long) (unsigned char) (s[pos] - '0')) < 10) {
    result = result * 10 + c;
    ++pos;
  }
  *u = result;
  return pos;
}

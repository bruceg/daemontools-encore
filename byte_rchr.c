/* Public domain. */

#include "byte.h"

unsigned int byte_rchr(const char *s,unsigned int n,int c)
{
  char ch;
  const char *t;
  const char *u;

  ch = c;
  t = s;
  u = 0;
  for (;;) {
    if (!n) break; if (*t == ch) u = t; ++t; --n;
    if (!n) break; if (*t == ch) u = t; ++t; --n;
    if (!n) break; if (*t == ch) u = t; ++t; --n;
    if (!n) break; if (*t == ch) u = t; ++t; --n;
  }
  if (!u) u = t;
  return u - s;
}

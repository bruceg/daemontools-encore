/* Public domain. */

#include "hasmemrchr.h"
#include "byte.h"

unsigned int byte_rchr(const char *s,unsigned int n,int c)
{
#ifdef HASMEMRCHR
  const char *u = memrchr(s,c,n);
  return (u == NULL) ? n : u - s;
#else
  char ch;
  const char *t;
  const char *u;

  ch = c;
  t = s;
  u = 0;
  for (;;) {
    if (!n) {break;} if (*t == ch) {u = t;} ++t; --n;
    if (!n) {break;} if (*t == ch) {u = t;} ++t; --n;
    if (!n) {break;} if (*t == ch) {u = t;} ++t; --n;
    if (!n) {break;} if (*t == ch) {u = t;} ++t; --n;
  }
  if (!u) u = t;
  return u - s;
#endif
}

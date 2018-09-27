/* Public domain. */

#include "str.h"

unsigned int str_chr(const char *s,int c)
{
  char ch;
  const char *t;

  ch = c;
  t = s;
  for (;;) {
    if (!*t) {break;} if (*t == ch) {break;} ++t;
    if (!*t) {break;} if (*t == ch) {break;} ++t;
    if (!*t) {break;} if (*t == ch) {break;} ++t;
    if (!*t) {break;} if (*t == ch) {break;} ++t;
  }
  return t - s;
}

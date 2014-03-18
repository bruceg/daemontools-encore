#include <fnmatch.h>
#include "byte.h"
#include "match.h"

int match_simple(const char *pattern,const char *buf,unsigned int len)
{
  char ch;

  for (;;) {
    ch = *pattern++;
    if (!ch) return !len;
    if (ch == '*') {
      ch = *pattern;
      if (!ch) return 1;
      for (;;) {
        if (!len) return 0;
        if (*buf == ch) break;
        ++buf; --len;
      }
      continue;
    }
    if (!len) return 0;
    if (*buf != ch) return 0;
    ++buf; --len;
  }
}

int match_fnmatch(const char *pattern,const char *buf,unsigned int len)
{
  char tmp[len+1];
  byte_copy(tmp,len,buf);
  tmp[len] = 0;
  return fnmatch(pattern,tmp,0) == 0;
}

int (*match)(const char *pattern,const char *buf,unsigned int len) = match_simple;

/* Public domain. */

#include "str.h"
#include "env.h"
#include "unconst.h"

extern /*@null@*/char *env_get(const char *s)
{
  int i;
  unsigned int len;

  if (!s) return 0;
  len = str_len(s);
  for (i = 0;environ[i];++i)
    if (str_start(environ[i],s) && (environ[i][len] == '='))
      return UNCONST(char *, environ[i] + len + 1);
  return 0;
}

#include <unistd.h>
#include "conf_supervise.c"
#include "env.h"
#include "stralloc.h"
#include "svpath.h"

static stralloc svdir;

int svpath_init(void)
{
  const char *base = env_get("SUPERVISEDIR");
  if (base == 0)
    base = conf_supervise;
  if (!stralloc_copys(&svdir,base)) return 0;
  if (base[0] == '/') {
    char cwd[8192];
    char *ptr;
    if (getcwd(cwd,sizeof cwd) == 0)
      return 0;
    for (ptr = cwd+1; *ptr != 0; ++ptr)
      if (*ptr == '/')
	*ptr = ':';
    if (!stralloc_cats(&svdir, cwd))
      return 0;
  }
  return 1;
}

int svpath_copy(stralloc *s,const char *suffix)
{
  return stralloc_copy(s,&svdir)
    && stralloc_cats(s,suffix)
    && stralloc_0(s);
}

const char *svpath_make(const char *suffix)
{
  static stralloc fntemp;
  return svpath_copy(&fntemp,suffix) ? fntemp.s : 0;
}

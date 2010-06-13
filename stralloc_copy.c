#include "stralloc.h"

int stralloc_copy(stralloc *sa,const stralloc *s)
{
  return stralloc_copyb(sa,s->s,s->len);
}

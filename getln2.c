#include "buffer.h"
#include "stralloc.h"
#include "byte.h"
#include "getln.h"

int getln2(buffer *buf,stralloc *sa,
	   /*@out@*/char **cont,/*@out@*/unsigned int *clen,int sep)
{
  char *x;
  unsigned int i;
  int n;
 
  if (!stralloc_ready(sa,0)) return -1;
  sa->len = 0;
 
  for (;;) {
    n = buffer_feed(buf);
    if (n < 0) return -1;
    if (n == 0) { *clen = 0; return 0; }
    x = buffer_PEEK(buf);
    i = byte_chr(x,n,sep);
    if (i < n) { buffer_SEEK(buf,*clen = i + 1); *cont = x; return 0; }
    if (!stralloc_readyplus(sa,n)) return -1;
    i = sa->len;
    sa->len = i + buffer_get(buf,sa->s + i,n);
  }
}

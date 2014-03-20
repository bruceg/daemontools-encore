#include "buffer.h"
#include "byte.h"

int buffer_gets(buffer *in,char *buf,unsigned int len,char eol,unsigned int *linelen)
{
  int n;
  const char *x;
  int i;

  while (len) {
    n = buffer_feed(in);
    if (n <= 0)
      return n;
    x = buffer_PEEK(in);
    i = byte_chr(x,n,eol);
    if (i == 0) {
      buffer_SEEK(in,1);
      break;
    }
    if (i > len)
      i = len;
    byte_copy(buf,i,x);
    buffer_SEEK(in,i);
    *linelen += i;
    buf += i;
    len -= i;
  }
  return 0;
}

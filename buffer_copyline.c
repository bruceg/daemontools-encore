#include "buffer.h"
#include "byte.h"

int buffer_copyline(buffer *out,buffer *in,char eol)
{
  int n;
  char *x;
  int i;
  for (;;) {
    n = buffer_feed(in);
    if (n < 0) return -2;
    if (!n) break;
    x = buffer_PEEK(in);
    i = byte_chr(x,n,eol);
    if (buffer_put(out,x,i) < 0) return -3;
    buffer_SEEK(in,i);
    if (i < in->p) {
      buffer_PUTC(out,eol);
      buffer_SEEK(in,1);
      break;
    }
  }
  return 0;
}

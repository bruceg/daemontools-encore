/* Public domain. */

#ifndef BYTE_H
#define BYTE_H

#include <string.h>

extern unsigned int byte_chr(const char *s,unsigned int n,int c);
extern unsigned int byte_rchr(const char *s,unsigned int n,int c);
#define byte_copy(TO,N,FROM) memcpy((TO),(FROM),(N))
#define byte_copyr(TO,N,FROM) memmove((TO),(FROM),(N))
#define byte_diff(S,N,T) memcmp((S),(T),(N))
#define byte_zero(S,N) memset((S),0,(N))

#define byte_equal(s,n,t) (!byte_diff((s),(n),(t)))

#endif

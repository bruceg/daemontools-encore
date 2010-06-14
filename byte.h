/* Public domain. */

#ifndef BYTE_H
#define BYTE_H

extern unsigned int byte_chr(const char *s,unsigned int n,int c);
extern unsigned int byte_rchr(const char *s,unsigned int n,int c);
extern void byte_copy(char *to,unsigned int n,const char *from);
extern void byte_copyr(char *to,unsigned int n,const char *from);
extern int byte_diff(const char *s,unsigned int n,const char *t);
extern void byte_zero(char *s,unsigned int n);

#define byte_equal(s,n,t) (!byte_diff((s),(n),(t)))

#endif

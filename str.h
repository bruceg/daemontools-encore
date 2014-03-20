/* Public domain. */

#ifndef STR_H
#define STR_H

extern int str_diff(const char *,const char *);
extern unsigned int str_len(const char *);
extern unsigned int str_chr(const char *,int);
extern int str_start(const char *,const char *);

#define str_equal(s,t) (!str_diff((s),(t)))

#endif

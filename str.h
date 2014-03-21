/* Public domain. */

#ifndef STR_H
#define STR_H

#include <string.h>

#define str_diff(S,T) strcmp((S),(T))
#define str_len(S) strlen(S)
extern unsigned int str_chr(const char *,int);
extern int str_start(const char *,const char *);

#define str_equal(s,t) (!str_diff((s),(t)))

#endif

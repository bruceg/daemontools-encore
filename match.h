#ifndef MATCH_H
#define MATCH_H

extern int match_simple(const char *,const char *,unsigned int);
extern int match_fnmatch(const char *,const char *,unsigned int);
extern int (*match)(const char *,const char *,unsigned int);

#endif

#ifndef SVPATH_H
#define SVPATH_H

struct stralloc;
extern int svpath_init(void);
extern int svpath_copy(struct stralloc *s,const char *suffix);
extern const char *svpath_make(const char *suffix);

#endif

#ifndef GETLN_H
#define GETLN_H

struct buffer;
struct stralloc;

extern int getln(struct buffer *buf,struct stralloc *sa,
		 int *match,int sep);
extern int getln2(struct buffer *buf,struct stralloc *sa,
		  /*@out@*/char **cont,/*@out@*/unsigned int *clen,int sep);

#endif

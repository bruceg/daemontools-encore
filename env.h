/* Public domain. */

#ifndef ENV_H
#define ENV_H

extern const char * const *environ;

extern /*@null@*/char *env_get(const char *);

#endif

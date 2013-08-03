#include <sys/types.h>

/*  http://gcc.gnu.org/bugzilla/show_bug.cgi?id=20229  */

#define UNCONST(type,x) ((type)(size_t)(x))


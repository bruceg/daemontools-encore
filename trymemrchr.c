/* Public domain. */

#define _GNU_SOURCE
#include <string.h>

int main()
{
  char try[1];
  memrchr(try,1,1);
  return 0;
}

#include <unistd.h>
#include "byte.h"
#include "open.h"
#include "error.h"
#include "direntry.h"
#include "stralloc.h"
#include "openreadclose.h"
#include "strerr.h"
#include "pathexec.h"
#include "sgetopt.h"

#define FATAL "envini: fatal: "

void die_usage(void)
{
  strerr_die1x(111,"envini: usage: envini [ -p prefix ] inifile child");
}
void nomem(void)
{
  strerr_die2x(111,FATAL,"out of memory");
}

static stralloc sa;
static stralloc name;
static stralloc section;
static stralloc value;
const char *prefix = "";

static int is_space(int ch)
{
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static void parse(void)
{
  int start;
  int end;
  int nl;
  int i;

  for (start = 0; start < sa.len; start = nl + 1) {
    nl = start + byte_chr(sa.s+start,sa.len-start,'\n');
    end = nl;
    while (end > start && is_space(sa.s[end-1]))
      --end;
    while (end > start && is_space(sa.s[start]))
      ++start;
    if (end > start) {
      if (sa.s[start] == '[' && sa.s[end-1] == ']') {
	if (!stralloc_copyb(&section,sa.s+start+1,end-start-2)) nomem();
	if (!stralloc_append(&section,'_')) nomem();
      }
      else if (sa.s[start] == ';')
	;
      else {
	i = start;
	while (i < end && sa.s[i] != '=' && !is_space(sa.s[i]))
	  ++i;
	if (!stralloc_copys(&name,prefix)) nomem();
	if (!stralloc_cat(&name,&section)) nomem();
	if (!stralloc_catb(&name,sa.s+start,i-start)) nomem();
	if (!stralloc_0(&name)) nomem();
	while (i < end && is_space(sa.s[i]))
	  ++i;
	if (i >= end || sa.s[i++] != '=')
	  continue;		/* Ignore misformatted lines */
	while (i < end && is_space(sa.s[i]))
	  ++i;
	if (!stralloc_copyb(&value,sa.s+i,end-i)) nomem();
	if (!stralloc_0(&value)) nomem();
	if (!pathexec_env(name.s,value.s)) nomem();
      }
    }
  }
}

int main(int argc,const char *const *argv)
{
  const char *fn;
  int opt;

  while ((opt = getopt(argc,argv,"p:")) != opteof)
    switch (opt) {
    case 'p': prefix = optarg; break;
    default: die_usage();
    }
  argv += optind;

  if (!*argv) die_usage();
  fn = *argv;

  if (!*++argv) die_usage();

  if (openreadclose(fn,&sa,1024) == -1)
    strerr_die3sys(111,FATAL,"unable to read ",fn);
  parse();

  pathexec(argv);
  strerr_die3sys(111,FATAL,"unable to run ",*argv);
}

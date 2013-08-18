#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "fmt.h"
#include "sig.h"
#include "str.h"
#include "open.h"
#include "taia.h"
#include "fifo.h"
#include "wait.h"
#include "error.h"
#include "ndelay.h"
#include "select.h"
#include "strerr.h"
#include "iopause.h"
#include "pathexec.h"

/* must be newline terminated */
const char banner[] = "fifostart\n";

#define BUFFER_SIZE 8192

int child = 0;
int nllast0 = 0;
int nllast1 = 0;
int nlfound0 = 0;
int nlfound1 = 0;
int exitsoon = 0;
int childdied = 0;
int fifo_rd = -1;
int fifo_wr = -1;

int fdself[2] = { -1, -1 };
int fdchild[2] = { -1, -1 };

const char *fn;
const char *const *saved_argv = 0;

/*
 * 'b' points at first byte of content
 * 'e' points at first free byte
 * 'n' points at last known newline
 * 'empty' signifies buffer empty
 * 'nl' signifies we know where last newline is
 * so...
 *    if empty == 1, b == e, buffer empty
 *    if empty == 0, b == e, buffer full
 *
 *    if e > b, can add (len - e) bytes.
 *    if e < b, can add (b - e) bytes.
 */
typedef struct fifobuffer {
  char *buf;
  unsigned int b;
  unsigned int e;
  unsigned int n;
  unsigned int len;
  int empty;
  int nl;
} fifobuffer;

#define FB_CANREAD(f) (        \
  (f)->empty                   \
    ? (f)->len - (f)->e        \
    : (                        \
        (f)->e > (f)->b        \
          ? (f)->len - (f)->e  \
          : (f)->b - (f)->e    \
    )                          \
)

#define FB_CANWRITE_NL(f) (            \
  (f)->empty                           \
  ? 0                                  \
  : (                                  \
      (f)->nl                          \
        ? (                            \
            ((f)->b <= (f)->n)         \
              ? ((f)->n - (f)->b + 1)  \
              : ((f)->len - (f)->b)    \
        )                              \
        : 0                            \
  )                                    \
)

#define FB_CANWRITE_ALL(f) (   \
  (f)->empty                   \
  ? 0                          \
  : (                          \
      ((f)->b < (f)->e)        \
        ? ((f)->e - (f)->b)    \
        : ((f)->len - (f)->b)  \
  )                            \
)

fifobuffer fifobuffer0;
fifobuffer fifobuffer1;

fifobuffer *fbuf_pref = 0;
fifobuffer *fbuf0 = &fifobuffer0;
fifobuffer *fbuf1 = &fifobuffer1;

char buf0[BUFFER_SIZE];
char buf1[BUFFER_SIZE];

#define FATAL "fifo: fatal: "
#define WARNING "fifo: warning: "
#define USAGE "fifo: usage: "

#define NDELAY_ON(t,m)            \
  if (ndelay_on((t)) == -1)       \
    strerr_die2sys(111,FATAL,(m));

#define NDELAY_OFF(t,m)           \
  if (ndelay_off((t)) == -1)      \
    strerr_die2sys(111,FATAL,(m));

#define PIPE(t,m)                  \
  if (pipe((t)) == -1)             \
    strerr_die2sys(111,FATAL,(m));

#define CLOSE(f,m)                     \
  for (;;) {                           \
    if (close((f)) != -1) break;       \
    if (errno == error_intr) continue; \
    strerr_die2sys(111,FATAL,(m));     \
  }

#define WRITE(f,b,l,m)                   \
  for (;;) {                             \
    if (write((f),(b),(l)) != -1) break; \
    if (errno == error_intr) continue;   \
    strerr_die2sys(111,FATAL,(m));       \
  }

#define FD_MOVE(t,f,m)                    \
  for (;;) {                              \
    if (my_fd_move((t),(f)) != -1) break; \
    if (errno == error_intr) continue;    \
    strerr_die2sys(111,FATAL,(m));        \
  }

#define OPEN_READ(d,f,m)                     \
  for (;;) {                                 \
    if (((d) = open_read((f))) != -1) break; \
    if (errno == error_intr) continue;       \
    strerr_die2sys(111,FATAL,(m));           \
  }

#define OPEN_WRITE(d,f,m)                     \
  for (;;) {                                  \
    if (((d) = open_write((f))) != -1) break; \
    if (errno == error_intr) continue;        \
    strerr_die2sys(111,FATAL,(m));            \
  }

#define FIFO_MAKE(f)                                         \
  {                                                          \
    struct stat sb;                                          \
    if (fifo_make((f),0600) == -1)                           \
      if (errno != error_exist)                              \
        strerr_die2sys(111,FATAL,"unable to create fifo: "); \
    if (stat((f),&sb) == -1)                                 \
      strerr_die2sys(111,FATAL,"unable to stat: ");          \
    if (!S_ISFIFO(sb.st_mode))                               \
      strerr_die3x(111,FATAL,"not a named pipe: ",(f));      \
  }

void die_usage(void)
{
  strerr_die2x(100,USAGE,"fifo file child");
}

void sig_child_handler(void)
{
  const char c = 'c';
  WRITE(fdself[1],&c,1,"unable to send c command: ");
}

void sig_alarm_handler(void)
{
  const char c = 'a';
  WRITE(fdself[1],&c,1,"unable to send a command: ");
}

void sig_term_handler(void)
{
  const char c = 't';
  WRITE(fdself[1],&c,1,"unable to send t command: ");
}

void buffer_init(fifobuffer *s,char *buf,unsigned int len)
{
  s->buf = buf;
  s->b = 0;
  s->e = 0;
  s->n = 0;
  s->len = len;
  s->nl = 0;
  s->empty = 1;
}

void millisleep(unsigned int s)
{
  struct taia now;
  struct taia deadline;
  iopause_fd x;

  taia_now(&now);
  deadline.sec.x = s / 1000;
  deadline.nano = (s * 1000) % 1000000000;
  deadline.atto = 0;
  taia_add(&deadline,&now,&deadline);

  for (;;) {
    taia_now(&now);
    if (taia_less(&deadline,&now)) return;
    iopause(&x,0,&deadline,&now);
  }
}

/*
 * if the original unhandled close() got EINTR here, we wind up
 * dup'ing from source to...
 *   "the lowest numbered available file descriptor greater than or
 *    equal to"
 * ...the target. in other words, to the wrong place, successfully!
 */
int my_fd_copy(int to,int from)
{
  if (to == from) return 0;
  if (fcntl(from,F_GETFL,0) == -1) return -1;
  CLOSE(to,"unable to close destination: ");
  if (fcntl(from,F_DUPFD,to) == -1) return -1;
  return 0;
}

/*
 * if the original unhandled close() got EINTR here, we wind up with
 * an extra descriptor.
 */
int my_fd_move(int to,int from)
{
  if (to == from) return 0;
  if (my_fd_copy(to,from) == -1) return -1;
  CLOSE(from,"unable to close source: ");
  return 0;
}

void do_c(void)
{
  int wc,we,wstat,dead;
  char s[FMT_ULONG];

  for (;;) {
    if ((dead = wait_nohang(&wstat)) != -1) break;
    if (errno == error_intr) continue;
    if (errno != error_child) break;
    strerr_die2x(111,FATAL,"reaping failed: ");
  }
  if (dead <= 0) {
    s[ fmt_uint(s,child) ] = '\0';
    strerr_warn3(WARNING,"child not dead? ",s,0);
    return;
  }
  if (child != dead) {
    s[ fmt_uint(s,dead) ] = '\0';
    strerr_warn3(WARNING,"unknown child: ",s,0);
    return;
  }

  wc = wait_crashed(wstat);
  we = wait_exitcode(wstat);

  if (!wc) {
    s[ fmt_uint(s,we) ] = '\0';
    strerr_warn3(WARNING,"child died: status ",s,0);
    if (we == 112)
      strerr_die2x(111,FATAL,"child died on start");
  }

  else if (wc > 0) {
    s[ fmt_uint(s,wc) ] = '\0';
    strerr_warn3(WARNING,"child died: signal ",s,0);
  }

  else {
    s[ fmt_uint(s,child) ] = '\0';
    strerr_die3x(111,FATAL,"child on fire: ",s);
  }

  childdied = 1;
}

void do_a(void)
{
  if (exitsoon)
    nlfound0 = nlfound1 = 1;
  else if (child)
    kill(child,sig_alarm);
}

void do_t(void)
{
  exitsoon = 1;
  alarm(2);
}

int check_pipe_empty(void)
{
  int r;
  fd_set rfds;
  struct timeval tv;

  for (;;) {
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(fdchild[0],&rfds);
    if ((r = select(fdchild[0]+1,&rfds,0,0,&tv)) != -1) break;
    if (errno == error_intr) continue;
    strerr_die2sys(111,FATAL,"unable to check pipe: ");
  }
  return( r ? 0 : 1 );
}

void do_self(void)
{
  int r;
  char c;
  if ((r = read(fdself[0],&c,1)) == -1) {
    if (errno == error_intr) return;
    if (errno == error_again) return;
    strerr_die2sys(111,FATAL,"unable to check self: ");
  }
  if (r >= 0) {
    switch (c) {
      case 'c': do_c(); return;
      case 'a': do_a(); return;
      case 't': do_t(); return;
    }
  }
  strerr_die2x(111,FATAL,"unable to grasp reality");
}

void check_self(void)
{
  int r;
  fd_set rfds;
  struct timeval tv;

  for (;;) {
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(fdself[0],&rfds);
    if ((r = select(fdself[0]+1,&rfds,0,0,&tv)) != -1) break;
    if (errno == error_intr) continue;
    strerr_die2sys(111,FATAL,"unable to check self: ");
  }
  if (!r) return;
  if (FD_ISSET(fdself[0],&rfds)) do_self();
}

void fb_to_child(fifobuffer *fb,const char *src,int *nlfound,int nllast)
{
  int len,w;

  if (fbuf_pref && fbuf_pref != fb) return;
  if (fb->empty) return;

  fbuf_pref = 0;

  /* Here be dragons */

  if (fb->nl) { /* normally, send up to last newline */
    if (!(len = FB_CANWRITE_NL(fb))) return;
    if ((w = write(fdchild[1],&fb->buf[fb->b],len)) == -1) {
      if (errno == error_intr) { fbuf_pref = fb; return; }
      strerr_die4sys(111,FATAL,"unable to flush ",src," with newline: ");
    }
    fb->b += w; fb->b %= fb->len;
    if (fb->b == fb->n + 1 || (!fb->b && fb->n == fb->len - 1)) fb->nl = 0;
    else fbuf_pref = fb;
    if (fb->b == fb->e) fb->empty = 1;
    if (fb->empty) fb->b = fb->e = fb->n = fb->nl = 0;
    return;
  }

  else if (!fb->empty && fb->b == fb->e) { /* buffer full, no newline */
    if (!(len = FB_CANWRITE_ALL(fb))) return;
    if ((w = write(fdchild[1],&fb->buf[fb->b],len)) == -1) {
      if (errno == error_intr) { fbuf_pref = fb; return; }
      strerr_die4sys(111,FATAL,"unable to flush ",src," without newline: ");
    }
    fb->b += w; fb->b %= fb->len;
    if (fb->b == fb->e) fb->empty = 1;
    if (fb->empty) fb->b = fb->e = fb->n = fb->nl = 0;
    fbuf_pref = fb;
    return;
  }

  else if (*nlfound) { /* all done, flush it all, add a newline if needed */
    if (!(len = FB_CANWRITE_ALL(fb))) return;
    if ((w = write(fdchild[1],&fb->buf[fb->b],len)) == -1) {
      if (errno == error_intr) { fbuf_pref = fb; return; }
      strerr_die4sys(111,FATAL,"unable to flush ",src,": ");
    }
    fb->b += w; fb->b %= fb->len;
    if (fb->b == fb->e) fb->empty = 1;
    else fbuf_pref = fb;
    if (fb->empty && !nllast) {
      fb->buf[0] = '\n';
      fb->e = fb->nl = 1;
      fb->b = fb->n = fb->empty = 0;
      fbuf_pref = fb;
    }
  }
}

void do_child(void)
{
  fb_to_child(fbuf0,"stdin",&nlfound0,nllast0);
  fb_to_child(fbuf1,"fifo",&nlfound1,nllast1);
}

void do_stdin(void)
{
  char *ptr;
  int len,r,x;

  if (childdied) return;

  len = exitsoon ? 1 : FB_CANREAD(fbuf0);
  if (len < 1) strerr_die2x(111,FATAL,"learn to count stdin!");

  if (fbuf0->empty) fbuf0->b = fbuf0->e = 0;

  ptr = fbuf0->buf + fbuf0->e;

  if ((r = read(0,ptr,len)) == -1) {
    if (errno == error_intr) return;
    if (errno == error_again) return; /* shouldnt happen */
    strerr_die2sys(111,FATAL,"unable to read stdin: ");
  }

  if (!r) { nlfound0 = 1; return; }

  fbuf0->empty = 0;

  fbuf0->e += r;
  fbuf0->e %= fbuf0->len;

  nllast0 = ptr[r-1] == '\n';

  for ( x=r-1 ; x>=0 ; --x ) {
    if (ptr[x] != '\n') continue;
    fbuf0->n = &ptr[x] - fbuf0->buf;
    fbuf0->nl = 1;
    break;
  }

  if (exitsoon) if (nllast0) nlfound0 = 1;
}

void do_fifo(void)
{
  char *ptr;
  int len,r,x;

  if (childdied) return;

  len = exitsoon ? 1 : FB_CANREAD(fbuf1);
  if (len < 1) strerr_die2x(111,FATAL,"learn to count fifo!");

  if (fbuf1->empty) fbuf1->b = fbuf1->e = 0;

  ptr = fbuf1->buf + fbuf1->e;

  if ((r = read(fifo_rd,ptr,len)) == -1) {
    if (errno == error_intr) return;
    if (errno == error_again) return;
    strerr_die2sys(111,FATAL,"unable to read fifo: ");
  }

  if (!r) return;

  fbuf1->empty = 0;

  fbuf1->e += r;
  fbuf1->e %= fbuf1->len;

  nllast1 = ptr[r-1] == '\n';

  for ( x=r-1 ; x>=0 ; --x) {
    if (ptr[x] != '\n') continue;
    fbuf1->n = &ptr[x] - fbuf1->buf;
    fbuf1->nl = 1;
    break;
  }

  if (exitsoon) if (nllast1) nlfound1 = 1;
}

void init_sigs(void)
{
  sig_catch( sig_child, sig_child_handler );
  sig_catch( sig_term,  sig_term_handler  );
  sig_catch( sig_alarm, sig_alarm_handler );
}

void init_fds(void)
{
  NDELAY_OFF(0,"unable to set blocking on stdin: ");

  FIFO_MAKE(fn);

  OPEN_READ(fifo_rd,fn,"unable to open fifo for read: ");
  OPEN_WRITE(fifo_wr,fn,"unable to open fifo for write: ");

  PIPE(fdself,"unable to create pipe to self: ");
  NDELAY_ON(fdself[0],"unable to set nonblocking on self: ");

  PIPE(fdchild,"unable to create pipe to child: ");
  FD_MOVE(1,fdchild[1],"unable to move pipe to stdout: ");
  fdchild[1] = 1;

  /* now we have:
   *  0 - stdin
   *  1 - fdchild[1]
   *  2 - stderr
   *  3 - fifo_rd
   *  4 - fifo_wr
   *  5 - fdself[0]
   *  6 - fdself[1]
   *  7 - fdchild[0]
   */
}

void init_buffers(void)
{
  buffer_init(fbuf0,buf0,sizeof buf0);
  buffer_init(fbuf1,buf1,sizeof buf1);
}

void fork_child(void)
{
  int null_wr = -1;
  for (;;) {
    childdied = 0;

    switch (child = fork()) {
      case -1:
        strerr_warn2(WARNING,"unable to fork: ",&strerr_sys);
        millisleep(1000);
        continue;

      case 0:
        CLOSE(fdself[0],"unable to close self reader: ");
        fdself[0] = -1;

        CLOSE(fdself[1],"unable to close self writer: ");
        fdself[1] = -1;

        CLOSE(fifo_rd,"unable to close forked reader: ");
        fifo_rd = -1;

        CLOSE(fifo_wr,"unable to close forked writer: ");
        fifo_wr = -1;

        FD_MOVE(0,fdchild[0],"unable to move pipe to stdin: ");
        fdchild[0] = 0;

        OPEN_WRITE(null_wr,"/dev/null","unable to reopen null writer: ");
        FD_MOVE(fdchild[1],null_wr,"unable to move null to stdout: ");
        null_wr = fdchild[1];
        fdchild[1] = -1;

        NDELAY_OFF(fdchild[0],"unable to set blocking on child: ");

        /* now we have:
         * 0 - fdchild[0]
         * 1 - /dev/null
         * 2 - stderr
         */

        pathexec(saved_argv);

        /* make this 112 so we can see it later in the parent */
        strerr_die4sys(112,WARNING,"unable to run ",*saved_argv,": ");
    }
    break;
  }
}

void write_banner(void)
{
  int wrote;
  const char *bptr = banner;
  int blen = str_len(banner);
  while (blen) {
    if ((wrote = write(fdchild[1],bptr,blen)) == -1) {
      if (errno == error_intr)
         continue;
      if (errno == error_again) {
        if (childdied)
          strerr_die2x(111,FATAL,"giving up on banner");
        millisleep(1);
        continue;
      }
      strerr_die2sys(111,FATAL,"unable to write banner: ");
    }
    bptr += wrote;
    blen -= wrote;
  }
}

void wait_banner(void)
{
  for (;;) {
    if (check_pipe_empty()) break;
    check_self();
    if (childdied)
      strerr_die2x(111,FATAL,"child died before banner");
    if (exitsoon) break;
    millisleep(10);
  }
}

void select_loop(void)
{
  int max;
  fd_set rfds,wfds;
  int add_fifo,add_child,add_stdin;

  for (;;) {
    if (nlfound0 && nlfound1 && fbuf0->empty && fbuf1->empty) break;

    add_fifo = add_stdin = 0;
    if (fbuf_pref != fbuf1 && FB_CANREAD(fbuf0) > 0) add_stdin = !nlfound0;
    if (fbuf_pref != fbuf0 && FB_CANREAD(fbuf1) > 0) add_fifo = !nlfound1;

    add_child = exitsoon
      ? ( FB_CANWRITE_ALL(fbuf0) > 0 || FB_CANWRITE_ALL(fbuf1) > 0 ? 1 : 0 )
      : ( FB_CANWRITE_NL(fbuf0)  > 0 || FB_CANWRITE_NL(fbuf1)  > 0 ? 1 : 0 );

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    FD_SET(fdself[0],&rfds);
    if (add_fifo)
      FD_SET(fifo_rd,&rfds);
    if (add_child)
      FD_SET(fdchild[1],&wfds);
    if (add_stdin)
      FD_SET(0,&rfds);

    max = fdself[0]; /* so, no need to worry about stdin */
    if (add_fifo) max = max > fifo_rd ? max : fifo_rd;
    if (add_child) max = max > fdchild[1] ? max : fdchild[1];

    if (select(max+1,&rfds,&wfds,0,0) == -1) {
      if (errno == error_intr) continue;
      strerr_die2sys(111,FATAL,"unable to select: ");
    }

    if (FD_ISSET(fdself[0],&rfds)) { do_self(); continue; }

    if (add_child && FD_ISSET(fdchild[1],&wfds)) do_child();
    if (add_stdin && FD_ISSET(0,&rfds)) do_stdin();
    if (add_fifo && FD_ISSET(fifo_rd,&rfds)) do_fifo();
  }
}

void stop_child(void)
{
  int wstat;

  if (!child) return;

  for (;;) {
    if (check_pipe_empty()) break;
    check_self();
    if (childdied) fork_child();
    millisleep(100);
  }

  sig_block(sig_child);
  CLOSE(fdchild[1],"unable to close pipe to child: ");
  fdchild[1] = -1;

  if (wait_pid(&wstat,child) == -1)
    if (errno != error_child)
      strerr_die2sys(111,FATAL,"waiting failed: ");
}

int main(int argc,const char *const *argv)
{
  if (!*argv) die_usage();
  if (!*++argv) die_usage();
  fn = *argv;

  if (!*++argv) die_usage();
  saved_argv = argv;

  init_sigs();
  init_fds();
  init_buffers();

  fork_child();

  write_banner();
  wait_banner();

  select_loop();

  stop_child();

  exit(0);
}

/*
SUMMARY
=======

    Log         Requests   KBytes     Relative Transfer Rates        Mean
Destination  |  / Second  / Second    null  |  file   |  fifo    Request Time
-----------  +  --------  --------  --------+---------+--------  ------------
   null      |  38507.95  34939.61  100.00% | 109.14% | 117.96%   259.687 ms
   file      |  35279.64  32014.20   91.63% | 100.00% | 108.08%   283.450 ms
   fifo      |  32860.74  29619.86   84.77% |  92.52% | 100.00%   304.315 ms

All tests performed using ApacheBench 2.3 accessing the vanilla, static
"Welcome to nginx!" page on localhost on a lightly loaded dual-core
AMD64 at 3.2GHz. Prior to each test it was ensured that the machine was
not using swap space.

In each test ApacheBench was run using keep-alives for 1,000,000 requests
at a concurrency of 10,000 parallel requests.

ab -n 1000000 -c 10000 -k -r http://localhost:1880/

No errors or non-200 pages were noted in any of the tests, either in
ApacheBench results or in the logs.

All file tests were performed starting with an empty log file.

All FIFO tests were performed starting with an empty log directory.

Details below...

*/

/*
# logging to null device
access_log /dev/null;
----
top - 03:11:29 up 12 days,  9:25, 12 users,  load average: 0.38, 0.30, 0.24
Tasks: 184 total,   3 running, 181 sleeping,   0 stopped,   0 zombie
Cpu0  : 31.7%us, 41.8%sy,  0.0%ni, 11.3%id,  0.0%wa,  0.0%hi, 15.2%si,  0.0%st
Cpu1  : 33.1%us, 44.2%sy,  0.0%ni,  9.0%id,  0.0%wa,  0.0%hi, 13.7%si,  0.0%st
Mem:   3915340k total,  3449444k used,   465896k free,   884732k buffers
Swap:  1048572k total,        0k used,  1048572k free,  1835676k cached

  PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+  COMMAND
 6428 nginx     20   0 48404  15m  892 R   99  0.4   0:08.11 nginx
 6430 luser     20   0  156m  78m  960 R   77  2.0   0:06.51 ab
----
Server Software:        nginx/1.4.2
Server Hostname:        localhost
Server Port:            1880

Document Path:          /
Document Length:        612 bytes

Concurrency Level:      10000
Time taken for tests:   25.969 seconds
Complete requests:      1000000
Failed requests:        0
Write errors:           0
Keep-Alive requests:    995628
Total transferred:      929110942 bytes
HTML transferred:       616081428 bytes
Requests per second:    38507.95 [#/sec] (mean)
Time per request:       259.687 [ms] (mean)
Time per request:       0.026 [ms] (mean, across all concurrent requests)
Transfer rate:          34939.61 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    2  18.9      0     243
Processing:     0  252 235.2    253     658
Waiting:        0  252 234.8    253     526
Total:          0  254 235.0    334     658

Percentage of the requests served within a certain time (ms)
  50%    334
  66%    479
  75%    492
  80%    499
  90%    505
  95%    508
  98%    509
  99%    511
 100%    658 (longest request)
*/

/*
# logging to file
access_log logs/access.log;
----
-rw-r--r-- 1 nginx nginx     0 Jul 26 03:13 access.log
-rw-r--r-- 1 nginx nginx 14035 Jul 20 21:19 error.log
----
top - 03:14:41 up 12 days,  9:28, 12 users,  load average: 0.27, 0.26, 0.24
Tasks: 184 total,   4 running, 180 sleeping,   0 stopped,   0 zombie
Cpu0  : 32.0%us, 52.7%sy,  0.0%ni,  2.3%id,  0.0%wa,  0.0%hi, 12.9%si,  0.0%st
Cpu1  : 26.5%us, 33.6%sy,  0.0%ni, 25.6%id,  0.2%wa,  0.0%hi, 14.1%si,  0.0%st
Mem:   3915340k total,  3302892k used,   612448k free,   884764k buffers
Swap:  1048572k total,        0k used,  1048572k free,  1691092k cached

  PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+  COMMAND
 6448 nginx     20   0 48404  15m  888 R  100  0.4   0:10.45 nginx
 6450 luser     20   0  156m  80m  960 R   70  2.1   0:07.58 ab
----
Server Software:        nginx/1.4.2
Server Hostname:        localhost
Server Port:            1880

Document Path:          /
Document Length:        612 bytes

Concurrency Level:      10000
Time taken for tests:   28.345 seconds
Complete requests:      1000000
Failed requests:        0
Write errors:           0
Keep-Alive requests:    996389
Total transferred:      929219924 bytes
HTML transferred:       616151196 bytes
Requests per second:    35279.64 [#/sec] (mean)
Time per request:       283.450 [ms] (mean)
Time per request:       0.028 [ms] (mean, across all concurrent requests)
Transfer rate:          32014.20 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    2  18.6      0     241
Processing:     0  275 259.3    314     635
Waiting:        0  275 259.0    314     604
Total:          0  277 259.0    341     635

Percentage of the requests served within a certain time (ms)
  50%    341
  66%    525
  75%    543
  80%    548
  90%    554
  95%    558
  98%    562
  99%    570
 100%    635 (longest request)
----
-rw-r--r-- 1 nginx nginx 91020690 Jul 26 03:15 access.log
-rw-r--r-- 1 nginx nginx    14035 Jul 20 21:19 error.log
*/

/*
# using fifo linked to multilog t '-* fifostart' s10000000 n400 ./main
access_log fifo;
----
$> cat run
#!/bin/sh
exec 2>&1
exec \
    env - PATH=/usr/local/bin \
    /usr/local/sbin/setuidgid nginxlog \
    fifo logpipe \
    multilog t '-* fifostart' s10000000 n400 ./main
----
-rw-r--r-- 1 nginxlog nginxlog 0 Jul 26 05:18 current
-rw------- 1 nginxlog nginxlog 0 Jul 20 10:59 lock
-rw-r--r-- 1 nginxlog nginxlog 0 Jul 20 10:59 state
----
top - 05:19:44 up 12 days, 11:33, 12 users,  load average: 0.65, 0.41, 0.21
Tasks: 245 total,   4 running, 241 sleeping,   0 stopped,   0 zombie
Cpu0  : 38.7%us, 49.2%sy,  0.0%ni,  3.0%id,  0.2%wa,  0.0%hi,  9.0%si,  0.0%st
Cpu1  : 39.2%us, 45.7%sy,  0.0%ni,  0.0%id,  2.8%wa,  0.0%hi, 12.3%si,  0.0%st
Mem:   3915340k total,  3476592k used,   438748k free,   886196k buffers
Swap:  1048572k total,        0k used,  1048572k free,  1805424k cached

  PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+  COMMAND
 6509 nginx     20   0 48440  15m  900 R   68  0.4   1:23.39 nginx
 9332 luser     20   0  156m  84m  956 R   61  2.2   0:09.48 ab
 6508 nginx     20   0 43152  10m  896 S   35  0.3   1:27.47 nginx
 9304 nginxlog  20   0  4200  476  384 S   16  0.0   0:12.14 multilog
 9303 nginxlog  20   0  6140  328  256 S   13  0.0   0:09.39 fifo
----
Server Software:        nginx/1.4.2
Server Hostname:        localhost
Server Port:            1880

Document Path:          /
Document Length:        612 bytes

Concurrency Level:      10000
Time taken for tests:   30.431 seconds
Complete requests:      1000000
Failed requests:        0
Write errors:           0
Keep-Alive requests:    997077
Total transferred:      923008460 bytes
HTML transferred:       612015300 bytes
Requests per second:    32860.74 [#/sec] (mean)
Time per request:       304.315 [ms] (mean)
Time per request:       0.030 [ms] (mean, across all concurrent requests)
Transfer rate:          29619.86 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    2  18.1      0     240
Processing:     0  299 310.4     34     860
Waiting:        0  299 310.5     34     860
Total:          0  301 310.5     34     860

Percentage of the requests served within a certain time (ms)
  50%     34
  66%    581
  75%    613
  80%    630
  90%    671
  95%    728
  98%    783
  99%    808
 100%    860 (longest request)
----
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f2d3775bf0c.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f301d272c44.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f3303d51a1c.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f352816eaa4.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f380f413084.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f3a3304c4a4.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f3d1bac53e4.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f4005691be4.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f422ba5e594.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f45144d3e24.s
-rwxr--r-- 1 nginxlog nginxlog 9998040 Jul 26 05:19 @4000000051f23f47388e5ee4.s
-rw-r--r-- 1 nginxlog nginxlog 7184460 Jul 26 05:20 current
-rw------- 1 nginxlog nginxlog       0 Jul 20 10:59 lock
-rw-r--r-- 1 nginxlog nginxlog       0 Jul 20 10:59 state
*/

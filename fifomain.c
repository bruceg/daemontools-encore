#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "fmt.h"
#include "sig.h"
#include "str.h"
#include "taia.h"
#include "fifo.h"
#include "wait.h"
#include "error.h"
#include "buffer.h"
#include "ndelay.h"
#include "strerr.h"
#include "iopause.h"
#include "fionread.h"
#include "pathexec.h"

int pi[2];
int fifo_rd;
int fifo_wr;
int null_rd;
int null_wr;
int child = 0;
const char *fn;
int nllast = 0;
int nlfound = 0;
int exitsoon = 0;
int childdied = 0;
buffer ssin,ssout;
char inbuf[BUFFER_INSIZE];
char outbuf[BUFFER_OUTSIZE];
const char *const *save_argv = NULL;

/* must be newline and null-terminated */
char banner[] = "fifostart\n\0";

#define FATAL "fifo: fatal: "
#define WARNING "fifo: warning: "
#define USAGE "fifo: usage: "

void die_usage(void)
{
  strerr_die2x(100,USAGE,"fifo file child");
}

void sig_term_handler(void)
{
  exitsoon = 1;
  alarm(2);
}

void sig_alarm_handler(void)
{
  if (exitsoon) {
    nlfound = 1;
    return;
  }
  if (child)
    kill(child,sig_alarm);
}

void sig_child_handler(void)
{
  int len;
  int wstat;
  if (child) {
    char s[FMT_ULONG];
    for (;;) {
      if (wait_pid(&wstat,child) == -1) {
        if (errno == error_intr) continue;
        if (errno == error_child) break;
        strerr_die2x(111,FATAL,"reaping failed: ");
      }
      break;
    }
    if ((wstat & 0x7f) == 0) {
      len = fmt_uint(s,((wstat & 0xff00) >> 8));
      s[len] = '\0';
      strerr_warn3(WARNING,"child died: status ",s,0);
      childdied = 1;
      return;
    }
    if (((signed char) (((wstat) & 0x7f) + 1) >> 1) > 0) {
      len = fmt_uint(s,(wstat & 0x7f));
      s[len] = '\0';
      strerr_warn3(WARNING,"child died: signal ",s,0);
      childdied = 1;
      return;
    }
    strerr_die2x(111,FATAL,"child on fire");
  }
  strerr_die2x(111,FATAL,"immaculate conception");
}

void millisleep(unsigned int s)
{
  struct taia now;
  struct taia deadline;
  iopause_fd x;
  uint64 secs;
  unsigned long nsecs;

  secs = s / 1000;
  nsecs = (s * 1000) % 1000000000;

  taia_now(&now);
  /* taia_uint(&deadline,s); */
  deadline.sec.x = secs;
  deadline.nano = nsecs;
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
 * dup'ing from source to `the lowest numbered available file descriptor
 * greater than or equal to' the target. in other words, the wrong place,
 * successfully.
 */
int my_fd_copy(int to,int from)
{
  if (to == from) return 0;
  if (fcntl(from,F_GETFL,0) == -1) return -1;
  for (;;) {
    if (close(to) != -1) break;
    if (errno == error_intr) continue;
    return -1;
  }
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
  for (;;) {
    if (close(from) != -1) break;
    if (errno == error_intr) continue;
    return -1;
  }
  return 0;
}

void fork_child(void)
{
  for (;;) {
    childdied = 0;
    switch (child = fork()) {

      case -1:
        strerr_warn2(WARNING,"unable to fork: ",&strerr_sys);
        millisleep(1000);
        continue;

      case 0:
        for (;;) {
          if (my_fd_move(0,pi[0]) != -1) break;
          if (errno == error_intr) continue;
          strerr_die2sys(111,FATAL,"unable to move pipe to stdin: ");
        }
        pi[0] = 0;

        for (;;) {
          if (my_fd_move(1,null_wr) != -1) break;
          if (errno == error_intr) continue;
          strerr_die2sys(111,FATAL,"unable to move null to stdout: ");
        }
        null_wr = 1;

        for (;;) {
          if (close(fifo_rd) != -1) break;
          if (errno == error_intr) continue;
          strerr_die2sys(111,FATAL,"unable to close forked reader: ");
        }

        for (;;) {
          if (close(fifo_wr) != -1) break;
          if (errno == error_intr) continue;
          strerr_die2sys(111,FATAL,"unable to close forked writer: ");
        }

        pathexec(save_argv);
        strerr_die4sys(111,WARNING,"unable to run ",*save_argv,": ");
    }
    break;
  }
}

/* buffer notes
 *   s->x points to next available buffer space
 *   s->n is how much space available
 *   s->op and s->fd are obvious
 *   s->p is buffer space used
 */

int myread(int fd,char *buf,int len)
{
  int r;
  int new_rd;
  /*
   * buffer_flush() returns either 0 or -1 on write() error
   *
   * buffer_flush() assumes all data is written. if any write() error
   * other than EINTR occurs, it is propogated back to us. ssout->p has
   * already been reset to 0, so any unwritten data in ssout is lost!
   *
   * or is it? ssout->x nor ssout->n are changed, so... in theory we
   * could semi-gracefully recover, but the errors from write() all look
   * pretty catastrophic, so just die.
   */
  if (buffer_flush(&ssout) == -1)
    strerr_die2sys(111,FATAL,"unable to flush: ");
  /*
   * at this point we need to know several things: exitsoon, childdied,
   * nlfound, nllast.
   *
   * if childdied, we don't want to read any more until a new child is
   * forked, if any. just return 0.
   */
  if (childdied) return 0;
  /*
   * if we're exiting and what we've read so far was newline-terminated
   * already, we can start flushing now, so don't read in that
   * case. set newline found and return 0.
   */
  if (exitsoon && nllast) nlfound = 1;
  /* if we'll be exiting soon and newline has been found, we don't want
   * to read any more. we're on our way out, so just return 0.
   */
  if (nlfound) return 0;
  /*
   * if exitsoon, we want to read one character at a time up until the
   * next newline then let the buffer flush and close the pipe to child
   * so it exits, then ourselves exit.
   */
  if (exitsoon) len = 1;

  r = read(fd,buf,len);

  if (r > 0) {
    /*
     * check for newline-termination
     */
    nllast = (buf[r-1] == '\n') ? 1 : 0;
    return r;
  }
  if (!r) {
    /*
     * what we want here is to find out if we have anything buffered. if
     * so, we want to just return 0 and let it flush. if the write()
     * blocks, fine. we'll end up looping around to read again, at which
     * time we may have a new writer and not get here again.
     */
    if ((ssin.p != 0) || (ssout.p != 0)) return 0;
    /*
     * if nothing buffered, we want to somehow block until we have
     * some input. whether we block on open() here or loop back around
     * to block on read() doesn't matter, though the latter is preferred.
     */
    for (;;) {
      if ((new_rd = open(fn,O_RDONLY)) != -1) break;
      if (errno == error_intr) continue;
      strerr_die2sys(111,FATAL,"unable to open new reader: ");
    }
    for (;;) {
      if (my_fd_move(fifo_rd,new_rd) != -1) break;
      if (errno == error_intr) continue;
      strerr_die2sys(111,FATAL,"unable to move new reader: ");
    }
  }
  return r;
}

int my_buffer_copy(buffer *out,buffer *in)
{
  int n;
  char *x;

  for (;;) {
    /* buffer_feed returns in->p if in->p without calling myread(),
       or whatever returned from myread() */
    /* myread() return either count read or -1 on read() OR write()
       error. error_intr already handled. */
    /* myread() returns 0 in cases: childdied or nlfound before read(),
       and end of file on fifo. */
    n = buffer_feed(in);
    if (n < 0) return -2;
    if (n) {
      x = buffer_PEEK(in);
      /* buffer_put returns -1 on catastrophic write() or 0 */
      if (buffer_put(out,x,n) == -1) return -3;
      buffer_SEEK(in,n);
    }
    if (childdied) fork_child();
    if ((nlfound != 0) && (in->p == 0) && (out->p == 0)) return 0;
  }
}

long check_pipe(void) {
#ifdef HASFIONREAD
  FIONREADTYPE remain;
  if (ioctl(1,FIONREAD,&remain) == -1)
    strerr_die2sys(111,FATAL,"unable to check pipe: ");
  return((long)remain);
#endif
  return 0;
}

int main(int argc,const char *const *argv)
{
  int blen;
  int wrote;
  char *bptr;

  if (!*argv) die_usage();
  if (!*++argv) die_usage();

  fn = *argv;

  if (!*++argv) die_usage();

  save_argv = argv;

  sig_catch(sig_child,sig_child_handler);
  sig_catch(sig_term,sig_term_handler);
  sig_catch(sig_alarm,sig_alarm_handler);

  if (fifo_make(fn,0600) == -1)
    if (errno != error_exist)
      strerr_die4sys(111,FATAL,"unable to create ",fn,": ");

  for (;;) {
    if ((fifo_rd = open(fn,O_RDONLY|O_NONBLOCK)) != -1) break;
    if (errno == error_intr) continue;
    strerr_die4sys(111,FATAL,"unable to open ",fn," for reading: ");
  }
  for (;;) {
    if ((fifo_wr = open(fn,O_WRONLY)) != -1) break;
    if (errno == error_intr) continue;
    strerr_die4sys(111,FATAL,"unable to open ",fn," for writing: ");
  }

  if (ndelay_off(fifo_rd) == -1)
    strerr_die2sys(111,FATAL,"unable to set blocking on fifo: ");

  for (;;) {
    if ((null_wr = open("/dev/null",O_WRONLY)) != -1) break;
    if (errno == error_intr) continue;
    strerr_die2sys(111,FATAL,"unable to open null writer: ");
  }
  for (;;) {
    if ((null_rd = open("/dev/null",O_RDONLY)) != -1) break;
    if (errno == error_intr) continue;
    strerr_die2sys(111,FATAL,"unable to open null reader: ");
  }

  if (pipe(pi) == -1)
    strerr_die2sys(111,FATAL,"unable to create pipe: ");

  for (;;) {
    if (my_fd_move(0,null_rd) != -1) break;
    if (errno == error_intr) continue;
    strerr_die2sys(111,FATAL,"unable to move null to stdin: ");
  }
  null_rd = 0;

  for (;;) {
    if (my_fd_move(1,pi[1]) != -1) break;
    if (errno == error_intr) continue;
    strerr_die2sys(111,FATAL,"unable to move pipe to stdout: ");
  }
  pi[1] = 1;

  buffer_init(&ssout,buffer_unixwrite,pi[1],outbuf,sizeof outbuf);
  buffer_init(&ssin,myread,fifo_rd,inbuf,sizeof inbuf);

  fork_child();

  bptr = banner;
  blen = str_len(banner);
  for (;;) {
    if ((wrote = write(pi[1],bptr,blen)) == -1) {
      if (errno == error_intr) continue;
      strerr_die2sys(111,FATAL,"unable to write banner: ");
    }
    if (wrote < blen) {
      bptr += wrote;
      blen -= wrote;
      continue;
    }
    break;
  }

  for (;;) {
    if (!check_pipe()) break;
    if (childdied)
      strerr_die2x(111,FATAL,"child died before banner");
    millisleep(100);
  }

  for (;;) {
    switch (my_buffer_copy(&ssout,&ssin)) {

      /* SIGTERM, newline, flushed, now close child */
      case 0:
        if (child) {
          int wstat = 0;
          for (;;) {
            if (!check_pipe()) break;
            if (childdied) fork_child();
            millisleep(100);
          }
          sig_block(sig_child);
          for (;;) {
            if (close(pi[1]) != -1) break;
            if (errno == error_intr) continue;
            strerr_die2sys(111,FATAL,"unable to close pipe to child: ");
          }
          if (wait_pid(&wstat,child) == -1)
            if (errno != error_child)
              strerr_die2sys(111,FATAL,"waiting failed: ");
        }
        exit(0);

      /* catastrophic error while reading fifo */
      case -2:
        strerr_die2sys(111,FATAL,"unable to feed: ");

      /* catastrophic error while writing pipe */
      case -3:
        strerr_die2sys(111,FATAL,"unable to write output: ");

    }
  }
  strerr_die2x(111,FATAL,"should never get here");
  return(0); /* make -Wall happy */
}

/*
SUMMARY
=======

    Log         Requests   KBytes                                    Mean
Destination  |  / Second  / Second    Relative Transfer Rates    Request Time
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

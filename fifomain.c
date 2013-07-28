#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ioctl.h> /* <sys/filio.h> on solaris */
#include "buffer.h"
#include "sig.h"
#include "taia.h"
#include "strerr.h"
#include "open.h"
#include "iopause.h"
#include "wait.h"
#include "pathexec.h"
#include "fifo.h"
#include "fmt.h"
#include "error.h"
#include "ndelay.h"
#include "fd.h"
#include "closeonexec.h"
#include "str.h"
#include "fionread.h"

#define FATAL "fifo: fatal: "

int child = 0;
const char *fn;
int exitsoon = 0;

char inbuf[BUFFER_INSIZE];
char outbuf[BUFFER_OUTSIZE];

buffer ssin;
buffer ssout = BUFFER_INIT(buffer_unixwrite,1,outbuf,sizeof outbuf);

int myread(fd,buf,len) int fd; char *buf; int len;
{
  if (buffer_flush(&ssout) == -1) return -1;
  if (exitsoon) return 0;
  return read(fd,buf,len);
}

void die_usage(void)
{
  strerr_die1x(100,"fifo: usage: fifo file child");
}

void sig_term_handler(void)
{
  exitsoon = 1;
}

void sig_alarm_handler(void)
{
  if (child)
    kill(child,sig_alarm);
}

void sig_child_handler(void)
{
  int len;
  int wstat;
  if (child) {
    char s[FMT_ULONG];
    do {
      if (wait_pid(&wstat,child) == -1)
        strerr_die1x(111,"waitpid failed in sig_child_handler");
      if ((wstat & 0x7f) == 0) {
        len = fmt_uint(s,((wstat & 0xff00) >> 8));
        s[len] = '\0';
        strerr_die2x(111,"child died: status ",s);
      }
      if (((signed char) (((wstat) & 0x7f) + 1) >> 1) > 0) {
        len = fmt_uint(s,(wstat & 0x7f));
        s[len] = '\0';
        strerr_die2x(111,"child died: signal ",s);
      }
    } while (
      !((wstat & 0x7f) == 0)
      && !(((signed char) ((wstat & 0x7f) + 1) >> 1) > 0)
    );
  }
  strerr_die2x(111,FATAL,"immaculate conception");
}

int my_buffer_copy(out,in)
register buffer *out;
register buffer *in;
{
  register int n;
  register char *x;

  for (;;) {
    n = buffer_feed(in);
    if (n < 0) return -2;
    if (exitsoon) return -4;
    if (!n) return 0;
    x = buffer_PEEK(in);
    if (buffer_put(out,x,n) == -1) return -3;
    buffer_SEEK(in,n);
    if (exitsoon) return -4;
  }
}

void millisleep(unsigned int s)
{
  struct taia now;
  struct taia deadline;
  iopause_fd x;
  int secs, nsecs;

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

int main(argc,argv) int argc; const char *const *argv;
{
  int fdr;
  int fdw;
  int blen;
  int pi[2];
  int rd_null;
  int wr_null;

  /* must be newline and null-terminated */
  char banner[] = "fifostart\n\0";

  blen = str_len(banner);

  if (!*argv) die_usage();
  if (!*++argv) die_usage();

  fn = *argv;

  if (!*++argv) die_usage();

  if (fifo_make(fn,0600) == -1)
    if (errno != error_exist)
      strerr_die4sys(111,FATAL,"unable to create ",fn,": ");

  if ((fdr = open_read(fn)) == -1)
    strerr_die4sys(111,FATAL,"unable to open ",fn," for reading: ");
  if ((fdw = open_write(fn)) == -1)
    strerr_die4sys(111,FATAL,"unable to open ",fn," for writing: ");

  if (ndelay_off(fdr))
    strerr_die2sys(111,FATAL,"unable to set nonblocking on fifo: ");

  buffer_init(&ssin,myread,fdr,inbuf,sizeof inbuf);

  if (pipe(pi))
    strerr_die2sys(111,FATAL,"unable to create pipe: ");
  if (ndelay_off(pi[1]))
    strerr_die2sys(111,FATAL,"unable to set nonblocking on pipe: ");
  if ((rd_null = open_read("/dev/null")) == -1)
    strerr_die2sys(111,FATAL,"unable to open null reader: ");
  if ((wr_null = open_write("/dev/null")) == -1)
    strerr_die2sys(111,FATAL,"unable to open null writer: ");

  sig_catch(sig_child,sig_child_handler);
  sig_catch(sig_term,sig_term_handler);
  sig_catch(sig_alarm,sig_alarm_handler);

  switch(child = fork()) {
    case -1:
      strerr_die2sys(111,FATAL,"unable to fork: ");
    case 0:
      if (fd_move(0,pi[0]))
        strerr_die2sys(111,FATAL,"unable to move pipe to stdin: ");
      if (fd_move(1,wr_null))
        strerr_die2sys(111,FATAL,"unable to move null to stdout: ");
      if (closeonexec(fdr))
        strerr_die2sys(111,FATAL,"unable to set fifo reader closeonexec: ");
      if (closeonexec(pi[1]))
        strerr_die2sys(111,FATAL,"unable to set pipe writer closeonexec: ");
      if (closeonexec(fdw))
        strerr_die2sys(111,FATAL,"unable to set fifo writer closeonexec: ");
      if (closeonexec(rd_null))
        strerr_die2sys(111,FATAL,"unable to set null reader closeonexec: ");
      pathexec(argv);
      strerr_die4sys(111,FATAL,"unable to exec ",*argv,": ");
  }

  if (close(pi[0]))
    strerr_die2sys(111,FATAL,"unable to close pipe reader: ");
  if (fd_move(0,rd_null))
    strerr_die2sys(111,FATAL,"unable to move null to stdin: ");
  if (fd_move(1,pi[1]))
    strerr_die2sys(111,FATAL,"unable to move pipe to stdout: ");
  if (close(wr_null))
    strerr_die2sys(111,FATAL,"unable to close null writer: ");

  write(1,&banner,blen);

#ifdef HASFIONREAD
  millisleep(1);
  while (1) {
    int remain;
    if (ioctl(1,FIONREAD,&remain) == -1)
      strerr_die2sys(111,FATAL,"unable to check pipe: ");
    if (remain < blen) break;
    millisleep(5);
  }
#endif

  switch (my_buffer_copy(&ssout,&ssin)) {
    case -2:
      strerr_die4sys(111,FATAL,"unable to read ",fn,": ");
    case -3:
      strerr_die2sys(111,FATAL,"unable to write output: ");
    case -4:
      if (1) {
        int wstat = 0;
        if (child) {
          sig_block(sig_child);
          if (close(1))
            strerr_die2sys(111,FATAL,"unable to close pipe to child: ");
          if (wait_pid(&wstat,child) == -1)
            strerr_die2sys(111,FATAL,"waitpid failed: ");
        }
        return(0);
      }
    case 0:
      strerr_die3x(111,FATAL,"end of file on ",fn);
  }
  strerr_die1x(111,"should never get here");
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

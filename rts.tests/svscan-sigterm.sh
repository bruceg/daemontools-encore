# TODO:
#   * how do we know logs stop *after* main?
#
#   * if logging with multilog (which exits at end of stdin), will a new
#     multilog be created after the main supervise exits, but before the
#     log supervise is signaled? if so, can we avoid it?
#

# svc0 - no log
# svc1 - svscan-managed log
# svc2 - supervise-managed log

echo '--- svscan handles sigterm'
echo

mkdir test.boot          || die "Could not create test.boot"
mkdir test.boot/service  || die "Could not create test.boot/service"
mkdir test.boot/svc0     || die "Could not create test.boot/svc0"
mkdir test.boot/svc1     || die "Could not create test.boot/svc1"
mkdir test.boot/svc1/log || die "Could not create test.boot/svc1/log"
mkdir test.boot/svc2     || die "Could not create test.boot/svc2"

cd test.boot || die "Could not change to test.boot"

ln -s ../svc0 service || die "Could not link svc0"
ln -s ../svc1 service || die "Could not link svc1"
ln -s ../svc2 service || die "Could not link svc2"

catexe svscan <<'EOF' || die "Could not create svscan wrapper"
#!/bin/sh
PATH=`echo $PATH | cut -d':' -f2-`
exec env - PATH=$PATH svscan $@ &
echo $! > svscan.pid
EOF

# FIXME: this doesnt work. get the pid in svscanboot instead.
#catexe readproctitle <<'EOF' || die "Could not create readproctitle wrapper"
##!/bin/sh
#exec >readproctitle.log
#exec 2>&1
#PATH=`echo $PATH | cut -d':' -f2-`
#exec env - PATH=$PATH readproctitle $@ &
#echo $! > test.boot/readproctitle.pid
#EOF

test -x ../../svscanboot || die "Could not find svscanboot source"
sed -r                                      \
  -e 's,PATH=/,PATH=.:..:../..:../../..:/,' \
  -e 's,^exec 2?>.+,,'                      \
  -e 's,/command/svc -dx .+,,g'             \
  -e 's,/?service,service,g'                \
  -e 's,readproctitle..*,& \& \
,'                                          \
  -e '$a\
echo $! > readproctitle.pid'                \
  -e '$a\
wait'                                       \
< ../../svscanboot                          \
| catexe svscanboot
test -x svscanboot || die "Could not create svscanboot stub"

catexe svc0/run <<'EOF' || die "Could not create svc0/run script"
#!/bin/sh
echo svc0 ran         >> ../svc0.log
exec ../../../sleeper >> ../svc0.log
EOF

catexe svc1/run <<'EOF' || die "Could not create svc1/run script"
#!/bin/sh
echo svc1-main ran    >> ../svc1-main.log
exec ../../../sleeper >> ../svc1-main.log
EOF

catexe svc1/log/run <<'EOF' || die "Could not create svc1/log/run script"
#!/bin/sh
echo svc1-log ran        >> ../../svc1-log.log
exec ../../../../sleeper >> ../../svc1-log.log
EOF

catexe svc2/run <<'EOF' || die "Could not create svc2/run script"
#!/bin/sh
echo svc2-main ran    >> ../svc2-main.log
exec ../../../sleeper >> ../svc2-main.log
EOF

catexe svc2/log <<'EOF' || die "Could not create svc2/log script"
#!/bin/sh
echo svc2-log ran     >> ../svc2-log.log
exec ../../../sleeper >> ../svc2-log.log
EOF

timed_read() {
  for i in 10 9 8 7 6 5 4 3 2 1 0; do
    if [ -f $1 ]; then
      cat $1
      break
    fi
    if [ $i -eq 0 ]; then
      echo 0
      break
    fi
    sleep 1
  done
}

echo '--- svscanboot started'
./svscanboot service > svscanboot.log 2>&1 &
svscanbootpid=$!
echo

check_pid_sanity() {
  if [ `echo $1 | grep -E '^[1-9][0-9]{0,4}$' | wc -l` != "1" ] \
    || [ $1 -le 1 ]                                             \
    || [ $1 -ge 65536 ]
  then
    echo 0
  else
    echo $1
  fi
}

echo '--- svscanboot pid looks sane'
svscanbootpid=`check_pid_sanity $svscanbootpid`
[ "$svscanbootpid" != "0" ] || echo no
echo

echo '--- svscan started'
svscanpid=`timed_read svscan.pid`
echo

echo '--- svscan pid looks sane'
svscanpid=`check_pid_sanity $svscanpid`
[ "$svscanpid" != "0" ] || echo no
echo

echo '--- readproctitle started'
readproctitlepid=`timed_read readproctitle.pid`
echo

echo '--- readproctitle pid looks sane'
readproctitlepid=`check_pid_sanity $readproctitlepid`
[ "$readproctitlepid" != "0" ] || echo no
echo

echo '--- svscanboot is running'
if ! kill -0 $svscanbootpid; then
  echo no
  svscanbootpid=0
fi
echo

echo '--- svscan is running'
if ! kill -0 $svscanpid; then
  echo no
  svscanpid=0
fi
echo

echo '--- readproctitle is running'
if ! kill -0 $readproctitlepid; then
  echo no
  readproctitlepid=0
fi
echo

echo '--- supervise svc0 is running'
svok svc0 || echo no
echo

echo '--- supervise svc1 is running'
svok svc1 || echo no
echo

echo '--- supervise svc1/log is running'
svok svc1/log || echo no
echo

echo '--- supervise svc2 is running'
svok svc2 || echo no
echo

echo '--- sigterm sent'
kill -TERM $svscanpid || echo no
echo

timed_ok() {
  for i in 10 9 8 7 6 5 4 3 2 1 0; do
    if ! svok $1; then
      break
    fi
    if [ $i -eq 0 ]; then
      echo no
      svc -dx $1
      break
    fi
    sleep 1
  done
}

echo '--- supervise svc0 is down'
timed_ok svc0
echo

echo '--- supervise svc1 is down'
timed_ok svc1
echo

echo '--- supervise svc1/log is down'
timed_ok svc1/log
echo

echo '--- supervise svc2 is down'
timed_ok svc2
echo

timed_ng() {
  if [ $1 -ne 0 ]; then
    for i in 10 9 8 7 6 5 4 3 2 1 0; do
      if ! kill -0 $1 2>/dev/null; then
        break
      fi
      if [ $i -eq 0 ]; then
        echo no
        kill -HUP $1
        break
      fi
      sleep 1
    done
  fi
}

echo '--- svscan is stopped'
timed_ng $svscanpid
echo

echo '--- readproctitle is stopped'
timed_ng $readproctitlepid
echo

echo '--- svscanboot is stopped'
timed_ng $svscanbootpid
echo

echo '--- svscanboot log'
cat svscanboot.log
echo

echo '--- svc0 log'
cat svc0.log
echo

echo '--- svc1 main log'
cat svc1-main.log
echo

echo '--- svc1 log log'
cat svc1-log.log
echo

echo '--- svc2 main log'
cat svc2-main.log
echo

echo '--- svc2 log log'
cat svc2-log.log
echo

echo
cd $TOP


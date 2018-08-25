# TODO:
#   * how do we know logs stop *after* main?
#
#   * if logging with multilog (which exits at end of stdin), will a new
#     multilog be created after the main supervise exits, but before the
#     log supervise is signaled? if so, can we avoid it?
#

echo '--- svscan handles sigterm'
echo

mkdir test.boot          || die "Could not create test.boot"
mkdir test.boot/service  || die "Could not create test.boot/service"
mkdir test.boot/svc0     || die "Could not create test.boot/svc0"
mkdir test.boot/svc1     || die "Could not create test.boot/svc1"
mkdir test.boot/svc1/log || die "Could not create test.boot/svc1/log"

cd test.boot || die "Could not change to test.boot"

ln -s ../svc0 service || die "Could not link svc0"
ln -s ../svc1 service || die "Could not link svc1"

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

echo '--- svscanboot started'
./svscanboot service > svscanboot.log 2>&1 &
svscanbootpid=$!
echo

echo '--- svscanboot pid looks sane'
if [ `echo $svscanbootpid | grep -E '^[1-9][0-9]{0,4}$' | wc -l` != "1" ]; then
  echo no
  svscanbootpid=0
elif [ $svscanbootpid -le 1 ] || [ $svscanbootpid -ge 65536 ]; then
  echo no
  svscanbootpid=0
fi
echo

echo '--- svscan started'
for i in 10 9 8 7 6 5 4 3 2 1 0; do
  if [ -f svscan.pid ]; then
    svscanpid=`cat svscan.pid`
    break
  fi
  if [ $i -eq 0 ]; then
    echo no
    svscanpid=0
    break
  fi
  sleep 1
done
echo

echo '--- svscan pid looks sane'
if [ `echo $svscanpid | grep -E ^[1-9][0-9]*$ | wc -l` != '1' ]
then
  echo no
  svscanpid=0
elif [ $svscanpid -le 1 ] || [ $svscanpid -ge 65536 ]
then
  echo no
  svscanpid=0
fi
echo

echo '--- readproctitle started'
for i in 10 9 8 7 6 5 4 3 2 1 0; do
  if [ -f readproctitle.pid ]; then
    readproctitlepid=`cat readproctitle.pid`
    break
  fi
  if [ $i -eq 0 ]; then
    echo no
    readproctitlepid=0
    break
  fi
  sleep 1
done
echo

echo '--- readproctitle pid looks sane'
if [ `echo $readproctitlepid | grep -E ^[1-9][0-9]*$ | wc -l` != '1' ]
then
  echo no
  readproctitlepid=0
elif [ $readproctitlepid -le 1 ] || [ $readproctitlepid -ge 65536 ]
then
  echo no
  readproctitlepid=0
fi
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

echo '--- sigterm sent'
kill -TERM $svscanpid || echo no
echo

echo '--- supervise svc0 is down'
for i in 10 9 8 7 6 5 4 3 2 1 0; do
  if ! svok svc0; then
    break
  fi
  if [ $i -eq 0 ]; then
    echo no
    svc -dx svc0
    break
  fi
  sleep 1
done
echo

echo '--- supervise svc1 is down'
for i in 10 9 8 7 6 5 4 3 2 1 0; do
  if ! svok svc1; then
    break
  fi
  if [ $i -eq 0 ]; then
    echo no
    svc -dx svc1
    break
  fi
  sleep 1
done
echo

echo '--- supervise svc1/log is down'
for i in 10 9 8 7 6 5 4 3 2 1 0; do
  if ! svok svc1/log; then
    break
  fi
  if [ $i -eq 0 ]; then
    echo no
    svc -dx svc1/log
    break
  fi
  sleep 1
done
echo

echo '--- svscan is stopped'
if [ $svscanpid -ne 0 ]; then
  kill -0 $svscanpid 2>/dev/null \
  && echo no
fi
echo

echo '--- readproctitle is stopped'
if [ $readproctitlepid -ne 0 ]; then
  kill -0 $readproctitlepid 2>/dev/null \
  && echo no
fi
echo

echo '--- svscanboot is stopped'
if [ $svscanbootpid -ne 0 ]; then
  kill -0 $svscanbootpid 2>/dev/null \
  && echo no
fi
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

echo
cd $TOP


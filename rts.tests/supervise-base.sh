# not tested:

# supervise closes descriptors properly
# svc -p
# svscanboot

echo '--- supervise starts, svok works, svup works, svstat works, svc -x works'
supervise test.sv &
until svok test.sv
do
  sleep 1
done
svup test.sv; echo $?
svup -l test.sv; echo $?
svup -L test.sv; echo $?
( svstat test.sv; echo $?; ) | filter_svstat
svc -x test.sv; echo $?
wait
svstat test.sv; echo $?

echo '--- svc -ox works'
supervise test.sv &
until svok test.sv
do
  sleep 1
done
svc -ox test.sv
wait

echo '--- svstat and svup work for up services'
catexe test.sv/run <<EOF
#!/bin/sh
sleep 1
svstat .
echo $?
svstat -l .
echo $?
svstat -L .
echo $?
svup .
echo \$?
svup -L .
echo \$?
svup -l .
echo \$?
EOF
supervise test.sv | filter_svstat &
until svok test.sv
do
  sleep 1
done
svc -ox test.sv
wait

echo '--- svstat and svup work for logged services'
catexe test.sv/run <<EOF
#!/bin/sh
sleep 1
svstat .
echo $?
svstat -l .
echo $?
svstat -L .
echo $?
svup .
echo \$?
svup -L .
echo \$?
svup -l .
echo \$?
EOF
catexe test.sv/log <<EOF
#!/bin/sh
exec cat
EOF
supervise test.sv | filter_svstat &
until svok test.sv
do
  sleep 1
done
svc -Lolox test.sv
wait
rm -f test.sv/log

echo '--- svc -u works'
( echo '#!/bin/sh'; echo echo first; echo mv run2 run ) > test.sv/run
chmod 755 test.sv/run
( echo '#!/bin/sh'; echo echo second; echo svc -x . ) > test.sv/run2
chmod 755 test.sv/run2
supervise test.sv &
until svok test.sv
do
  sleep 1
done
svc -u test.sv
wait

echo '--- svc -o works first time with start'
catexe test.sv/run <<EOF
#!/bin/sh
echo ok > out
svc -dx .
EOF
catexe test.sv/start <<EOF
#!/bin/sh
exit 0
EOF
touch test.sv/down
supervise test.sv &
svpid=$!
until svok test.sv
do
  sleep 1
done
svc -o test.sv
for c in 1 2 3 4 5 6 7 8
do
  if [ -e test.sv/out ]
  then
    break
  fi
  sleep 1
done
if [ -e test.sv/out ]
then
  cat test.sv/out
else
  kill $svpid
fi
wait
rm -f test.sv/start test.sv/down test.sv/out

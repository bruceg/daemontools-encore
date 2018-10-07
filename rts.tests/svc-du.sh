svpid() {
  svstat test.sv | perl -ple 's/.+pid ([0-9]+).+/$1/ || s/.+/0/'
}

echo '--- svc -du works, service exits 0'
catexe test.sv/run <<EOF
#!/bin/sh
exec sleeper -x 0 -d 1000000
EOF
supervise test.sv &
sleep 1
pid1=`svpid`
svc -du test.sv
sleep 1
pid2=`svpid`
svc -xk test.sv
if [ "$pid1" = "0" ]; then
  echo no start
elif [ "$pid2" = "0" ]; then
  echo no restart
elif [ "$pid1" = "$pid2" ]; then
  echo same pid
else
  echo ok
fi
wait
echo

echo '--- svc -du works, service exits 1'
catexe test.sv/run <<EOF
#!/bin/sh
exec sleeper -x 1 -d 1000000
EOF
supervise test.sv &
sleep 1
pid1=`svpid`
svc -du test.sv
sleep 1
pid2=`svpid`
svc -xk test.sv
if [ "$pid1" = "0" ]; then
  echo no start
elif [ "$pid2" = "0" ]; then
  echo no restart
elif [ "$pid1" = "$pid2" ]; then
  echo same pid
else
  echo ok
fi
wait
echo

echo '--- svc -du works, service exits 100'
catexe test.sv/run <<EOF
#!/bin/sh
exec sleeper -x 100 -d 1000000
EOF
supervise test.sv &
sleep 1
pid1=`svpid`
svc -du test.sv
sleep 1
pid2=`svpid`
svc -xk test.sv
if [ "$pid1" = "0" ]; then
  echo no start
elif [ "$pid2" = "0" ]; then
  echo no restart
elif [ "$pid1" = "$pid2" ]; then
  echo same pid
else
  echo ok
fi
wait
echo

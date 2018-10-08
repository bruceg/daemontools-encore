svpid() {
  svstat test.sv \
  | sed -E -n 's/.+pid ([0-9][0-9]*).+/Z\1/; s/^[^Z].+/Z0/; s/Z//p'
}

echo '--- svc -du works, service exits 0'
makefifo sv.wait1 sv.wait2
catexe test.sv/run <<EOF
#!/bin/sh
mv run2 run
exec sleeper -x 0 -d 2500000000 -w ../sv.wait1
EOF
catexe test.sv/run2 <<EOF
#!/bin/sh
exec sleeper -x 0 -d 2500000000 -w ../sv.wait2
EOF
supervise test.sv &
cat sv.wait1
pid1=`svpid`
svc -du test.sv
cat sv.wait2
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
rm sv.wait1 sv.wait2
echo

echo '--- svc -du works, service exits 1'
makefifo sv.wait1 sv.wait2
catexe test.sv/run <<EOF
#!/bin/sh
mv run2 run
exec sleeper -x 1 -d 2500000000 -w ../sv.wait1
EOF
catexe test.sv/run2 <<EOF
#!/bin/sh
exec sleeper -x 1 -d 2500000000 -w ../sv.wait2
EOF
supervise test.sv &
cat sv.wait1
pid1=`svpid`
svc -du test.sv
cat sv.wait2
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
rm sv.wait1 sv.wait2
echo

echo '--- svc -du works, service exits 100'
makefifo sv.wait1 sv.wait2
catexe test.sv/run <<EOF
#!/bin/sh
mv run2 run
exec sleeper -x 100 -d 2500000000 -w ../sv.wait1
EOF
catexe test.sv/run2 <<EOF
#!/bin/sh
exec sleeper -x 100 -d 2500000000 -w ../sv.wait2
EOF
supervise test.sv &
cat sv.wait1
pid1=`svpid`
svc -du test.sv
cat sv.wait2
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
rm sv.wait1 sv.wait2
echo

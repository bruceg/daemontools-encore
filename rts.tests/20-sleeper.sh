echo '--- sleeper -w works'
sleeper -w
sleeper -w foo
makefifo sleeper.wait
catexe test.sv/run <<EOF
#!/bin/sh
exec sleeper -w ../sleeper.wait
EOF
supervise test.sv &
cat sleeper.wait
svc -dx test.sv
wait
rm sleeper.wait
echo

echo '--- sleeper -d works'
sleeper -d
sleeper -d foo
makefifo sleeper.wait
catexe test.sv/run <<EOF
#!/bin/sh
exec sleeper -d 1000000 -w ../sleeper.wait
EOF
supervise test.sv &
cat sleeper.wait
svc -dx test.sv
wait
rm sleeper.wait
echo

echo '--- sleeper -p works'
sleeper -p
sleeper -p foo
makefifo sleeper.wait sleeper.out
catexe test.sv/run <<EOF
#!/bin/sh
exec sleeper -p ../sleeper.out -w ../sleeper.wait
EOF
supervise test.sv &
cat sleeper.wait
svc -a test.sv
cat sleeper.out
svc -h test.sv
cat sleeper.out
svc -dx test.sv
cat sleeper.out
wait
rm sleeper.wait sleeper.out
echo

echo '--- sleeper -x works'
sleeper -x
sleeper -x foo
makefifo sleeper.wait
catexe test.sv/run <<EOF
#!/bin/sh
exec sleeper -x 100 -w ../sleeper.wait
EOF
supervise test.sv &
cat sleeper.wait
svc -xtc test.sv
wait
rm sleeper.wait

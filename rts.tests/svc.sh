( echo '#!/bin/sh'; echo 'exec ../../sleeper' ) > test.sv/run
chmod 755 test.sv/run

echo '--- svc sends right signals'
supervise test.sv &
sleep 1
svc -a test.sv
sleep 1
svc -c test.sv
sleep 1
svc -h test.sv
sleep 1
svc -i test.sv
sleep 1
svc -t test.sv
sleep 1
svc -q test.sv
sleep 1
svc -1 test.sv
sleep 1
svc -2 test.sv
sleep 1
svc -w test.sv
sleep 1
svc -d test.sv
sleep 1
svc -xk test.sv
wait

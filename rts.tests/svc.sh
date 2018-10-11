makefifo fifo.out fifo.wait
catexe test.sv/run <<EOF
#!/bin/sh
exec ../../sleeper -p ../fifo.out -w ../fifo.wait
EOF

echo '--- svc sends right signals'
supervise test.sv &
cat fifo.wait
svc -a test.sv
cat fifo.out
svc -c test.sv
svc -h test.sv
cat fifo.out
svc -c test.sv
svc -i test.sv
cat fifo.out
svc -t test.sv
svc -q test.sv
cat fifo.out
svc -1 test.sv
cat fifo.out
svc -2 test.sv
cat fifo.out
svc -w test.sv
cat fifo.out
svc -d test.sv
cat fifo.out
svc -xk test.sv
wait
rm fifo.out fifo.wait

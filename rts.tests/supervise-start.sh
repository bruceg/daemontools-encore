echo '--- svc -o works first time with start'
catexe test.sv/run <<EOF
#!/bin/sh
echo ok
svc -dx .
EOF
catexe test.sv/start <<EOF
#!/bin/sh
exit 0
EOF
touch test.sv/down
supervise test.sv &
until svok test.sv
do
  sleep 0
done
svc -o test.sv
wait
rm -f test.sv/start test.sv/down

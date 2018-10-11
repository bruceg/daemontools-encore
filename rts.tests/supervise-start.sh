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
waitok test.sv
svc -o test.sv
for c in 1 2 3 4 5 6 7 8
do
  if [ -r test.sv/out ]
  then
    break
  fi
  sleep 1
done
if [ -r test.sv/out ]
then
  cat test.sv/out
else
  kill $svpid
fi
wait
rm -f test.sv/start test.sv/down test.sv/out

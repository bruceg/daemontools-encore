echo '--- supervise has reasonable downtimes'

rm -rf test.sv/*

catexe test.sv/run <<EOF
#!/bin/sh
sleep 999
EOF

catexe test.sv/log <<EOF
#!/bin/sh
sleep 999
EOF

touch test.sv/down

supervise test.sv &
svpid=$!

until svok test.sv
do
  sleep 0
done

svstat test.sv \
| sed -r 's, \(.+\),,' \
| sed -r 's, ([0-9]|1[0-9]) second.+$, ok,'

kill $svpid
wait

rm -rf test.sv/*

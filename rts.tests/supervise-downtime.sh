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

waitok test.sv

svstat test.sv \
| sed 's, (..*),,' \
| sed 's! 1\{0,1\}[0-9] second..*$! ok!'

kill $svpid
wait

rm -rf test.sv/*

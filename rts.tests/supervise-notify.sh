echo --- supervise runs notify properly

catexe test.sv/notify <<EOF
#!/bin/sh
echo \$1 \$2 \$4
EOF

catexe test.sv/start <<EOF
#!/bin/sh
EOF

catexe test.sv/run <<EOF
#!/bin/sh
svc -dx .
exit 1
EOF

catexe test.sv/stop <<EOF
#!/bin/sh
EOF

supervise test.sv > test.log &
wait
sleep 1
# The lines can be output in different orders depending on the scheduler
sort test.log
rm -f test.sv/notify test.sv/start test.sv/stop

echo

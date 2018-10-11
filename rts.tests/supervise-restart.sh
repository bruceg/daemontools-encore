rm -f test.sv/down
catexe test.sv/start <<EOF
#!/bin/sh
echo started
EOF

for code in 0 1 100
do
    echo "--- supervise restarts service exiting $code"
    catexe test.sv/run2 <<EOF
#!/bin/sh
echo rerunning
exec sleep 999
EOF
    catexe test.sv/run <<EOF
#!/bin/sh
echo running
mv run2 run
exit $code
EOF
    supervise test.sv &
    waituntil svup test.sv
    sleep 3
    svc -xk test.sv
    wait
    echo
done
rm -f test.sv/start

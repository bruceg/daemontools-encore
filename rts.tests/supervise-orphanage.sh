echo '--- supervise properly runs an orphanage'
catexe test.sv/run <<EOF
#!/bin/sh
nohup ../../sleeper >output &
mv run2 run
echo the first run
exec ../../sleeper
EOF
catexe test.sv/run2 <<EOF
#!/bin/sh
echo the second run
svc -x .
EOF
touch test.sv/orphanage
supervise test.sv &
until svok test.sv || ! jobs %% >/dev/null 2>&1
do
  sleep 1
done
if svok test.sv
then
    svc -u test.sv
    while [ -e test.sv/run2 ]
    do
      sleep 1
    done
    svstat test.sv | filter_svstat
    svc -t test.sv
    until svstat test.sv | grep -q orphanage
    do
      sleep 1
    done
    svstat test.sv | filter_svstat
    cat test.sv/output
    svc -+h test.sv
    wait
    cat test.sv/output
else
    echo "This test fails on older Unix systems"
    echo "(everything which is not Linux or FreeBSD)"
    echo "as they have no subprocess reapers"
fi
rm test.sv/orphanage

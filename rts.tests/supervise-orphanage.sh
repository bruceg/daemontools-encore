echo '--- supervise properly runs an orphanage'
catexe test.sv/run <<EOF
#!/bin/sh
nohup ../../sleeper >output &
echo \$! >pid
mv run2 run
echo the first run is done
EOF
catexe test.sv/run2 <<EOF
#!/bin/sh
echo the second run
svc -x .
EOF
touch test.sv/subreaper
supervise test.sv &
until svok test.sv || ! jobs %% >/dev/null 2>&1
do
  sleep 1
done
if svok test.sv
then
    svc -u test.sv
    until [ -e test.sv/pid ]
    do
      sleep 1
    done
    svstat test.sv | filter_svstat
    until svstat test.sv | grep -q orphanage
    do
      sleep 1
    done
    svstat test.sv | filter_svstat
    cat test.sv/output
    kill $(cat test.sv/pid)
    wait
    cat test.sv/output
else
    echo "This test fails on older Unix systems"
    echo "(everything which is not Linux or FreeBSD)"
    echo "as they have no subprocess reapers"
fi
rm test.sv/subreaper

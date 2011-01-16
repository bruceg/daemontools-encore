# not tested:

# supervise closes descriptors properly
# svc -p
# svscanboot

echo '--- supervise starts, svok works, svstat works, svc -x works'
supervise test.sv &
until svok test.sv
do
  sleep 1
done
svup test.sv; echo $?
svstat test.sv | sed 's/[0-9]* seconds/x seconds/'; echo $?
svc -x test.sv; echo $?
wait
svstat test.sv; echo $?

echo '--- svc -ox works'
supervise test.sv &
until svok test.sv
do
  sleep 1
done
svc -ox test.sv
wait

echo '--- svstat works for up services'
( echo '#!/bin/sh'; echo sleep 1; echo svstat . ) > test.sv/run
chmod 755 test.sv/run
supervise test.sv \
| sed -e 's/[0-9]* seconds/x seconds/' -e 's/pid [0-9]*/pid x/' &
until svok test.sv
do
  sleep 1
done
svc -ox test.sv
wait

echo '--- svc -u works'
( echo '#!/bin/sh'; echo echo first; echo mv run2 run ) > test.sv/run
chmod 755 test.sv/run
( echo '#!/bin/sh'; echo echo second; echo svc -x . ) > test.sv/run2
chmod 755 test.sv/run2
supervise test.sv &
until svok test.sv
do
  sleep 1
done
svc -u test.sv
wait

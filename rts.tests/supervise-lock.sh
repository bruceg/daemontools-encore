echo '--- supervise leaves locked service intact'
supervise test.sv &
until svok test.sv
do
  sleep 0
done
( cd test.sv/supervise && ls -dl * | awk '{ print $1, $5, $9 }' )
supervise test.sv; echo $?
( cd test.sv/supervise && ls -dl * | awk '{ print $1, $5, $9 }' )
svc -x test.sv; echo $?
wait
svstat test.sv; echo $?

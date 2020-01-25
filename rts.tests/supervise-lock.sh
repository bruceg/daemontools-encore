echo '--- supervise leaves locked service intact'
supervise test.sv &
waitok test.sv
( cd test.sv/supervise && ls -dl * | awk '{ print substr($1,1,10), $5, $9 }' )
supervise test.sv; echo $?
( cd test.sv/supervise && ls -dl * | awk '{ print substr($1,1,10), $5, $9 }' )
svc -x test.sv; echo $?
wait
svstat test.sv; echo $?

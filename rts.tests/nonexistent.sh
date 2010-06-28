echo '--- svstat handles new and nonexistent directories'
( echo '#!/bin/sh'; echo echo hi ) > test.sv/run
chmod 755 test.sv/run
touch test.sv/down
svstat test.sv nonexistent; echo $?

echo '--- svc handles new and nonexistent directories'
svc test.sv nonexistent; echo $?

echo '--- svok handles new and nonexistent directories'
svok test.sv; echo $?
svok nonexistent; echo $?

echo '--- supervise handles nonexistent directories'
supervise nonexistent; echo $?

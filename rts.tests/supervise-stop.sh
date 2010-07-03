echo '--- supervise runs stop on down'
( echo '#!/bin/sh'; echo svc -dx . ) >test.sv/run
( echo '#!/bin/sh'; echo echo in stop ) >test.sv/stop
rm -f test.sv/down
chmod +x test.sv/run test.sv/stop
supervise test.sv &
wait
rm -f test.sv/stop

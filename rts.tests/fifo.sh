echo '--- fifo requires arguments'
fifo; echo $?


echo '--- fifo requires more arguments'
fifo pipe; echo $?


echo '--- fifo complains if it cannot run program'
fifo pipe ./nonexistent; echo $?


echo '--- fifo quits if child doesnt finish banner'
fifo pipe perl -e 'sysread(STDIN,$a,8); warn(qq{$a\n}); exit(111)'


echo '--- fifo runs a program'
fifo pipe multilog ./main &
fifopid=$!
echo 'hi there' > pipe
perl -e 'select(undef,undef,undef,0.5)'
cat main/current
kill $fifopid


echo '--- fifo runs a program but child dies later'
echo 'Ticking away the moments that make up the dull day,' > pipe
echo 'fritter and waste the hour in an offhand way.'       > pipe
fifo pipe perl -e 'sysread(STDIN, $a, 22) && warn( qq{:$a\n} )' &
fifopid=$!
perl -e 'select(undef,undef,undef,0.5)'
kill $fifopid
perl -e 'select(undef,undef,undef,0.5)'


echo '--- fifo requires arguments'
fifo; echo $?;


echo '--- fifo requires more arguments'
fifo pipe; echo $?


echo '--- fifo complains if it cannot run program'
fifo pipe ./nonexistent; echo $?; rm -f pipe


echo '--- fifo quits if child doesnt finish banner'
fifo pipe sh -c 'head -c 8 1>&2 ; echo 1>&2 ; exit 111'
rm -f pipe


echo '--- fifo runs a program'
fifo pipe multilog e &
echo 'hi there' > pipe
fifopid=$!
rm -f pipe
wait $fifopid


echo '--- fifo runs a program but child dies later'
mknod pipe p
( cat <<END
Ticking away the moments that make up the dull day,
fritter and waste the hour in an offhand way.
END
) > pipe &
fifo pipe sh -c 'head -c 22 1>&2 ; echo 1>&2 ; exit 121' &
fifopid=$!
sleep 1
kill -TERM $fifopid
wait $fifopid
rm -f pipe


echo '--- fifo requires arguments'
fifo; echo $?


echo '--- fifo requires more arguments'
fifo pipe; echo $?

echo '--- fifo complains if it cannot run program'
fifo pipe ./nonexistent; echo $?

echo '--- fifo runs a program'
fifo pipe multilog ./main &
echo $?
fifopid=$!
sleep 1
echo 'hi there' > pipe
cat main/current
sleep 1
kill $fifopid


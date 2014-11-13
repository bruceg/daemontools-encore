echo '--- fifo requires arguments'
fifo
echo $?

echo '--- fifo complains about files'
rm -f pipe
touch pipe
fifo pipe
echo $?

echo '--- fifo dies at end of pipe'
rm -f pipe fifo.out
fifo pipe > fifo.out 2>&1 &
fifopid=$!
echo $?
echo 'hi there' > pipe
cat fifo.out
kill $fifopid >/dev/null 2>&1
wait >/dev/null 2>&1

echo '--- fifo complains about reading'
chmod 000 pipe
fifo pipe
echo $?

echo '--- fifo complains about writing'
chmod 400 pipe
fifo pipe
echo $?


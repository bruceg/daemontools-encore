echo '--- fifo requires arguments'
fifo; echo $?;


echo '--- fifo requires more arguments'
fifo pipe; echo $?


echo '--- fifo creates pipe but complains if it cannot run program'
fifo pipe ./nonexistent
echo $?
if [ -p pipe ]; then echo '1'; else echo '0'; fi
rm -f pipe


echo '--- fifo quits if child does not finish banner'
fifo pipe sh -c 'head -c 8 1>&2 ; echo 1>&2 ; exit 121'
echo $?
rm -f pipe


echo '--- fifo runs a program'
mknod pipe p
fifo pipe multilog e &
fifopid=$!
echo 'hi there' > pipe
kill -TERM $fifopid
wait $fifopid
echo $?
rm -f pipe


echo '--- fifo runs a program but child dies later'
mknod pipe p
fifo pipe sh -c 'head -c 22 1>&2 ; echo 1>&2 ; exit 131' &
fifopid=$!
cat >pipe <<END
Ticking away the moments that make up the dull day,
fritter and waste the hour in an offhand way.
END
sleep 1
kill -TERM $fifopid
wait $fifopid
echo $?
rm -f pipe


echo '--- fifo stops at SIGTERM if newline was last'
mknod pipe p
fifo pipe sh -c 'head -c 70 1>&2 ; echo 1>&2 ; exit 141' &
fifopid=$!
cat >pipe <<END
Something has to change.
END
echo $?
kill -TERM $fifopid
cat >pipe <<END
Undeniable dilemma.
Boredoms not a burden
anyone should bear.
END
wait $fifopid
echo $?
rm -f pipe


echo '--- fifo continues after SIGTERM until newline'
mknod pipe p
fifo pipe sh -c 'cat -v 1>&2 ; echo 1>&2 ; exit 151' &
fifopid=$!
cat >pipe <<END
Whiskey, gin and brandy
With a glass I'm pretty handy
END
echo -n "I'm trying to walk " > pipe
sleep 1
kill -TERM $fifopid
echo 'a straight line' > pipe
echo 'On sour mash and cheap wine' > pipe
wait $fifopid
echo $?
rm -f pipe


echo '--- fifo continues after SIGTERM until newline, child dies early'
mknod pipe p
fifo pipe sh -c 'head -c 140 1>&2 ; echo 1>&2 ; sleep 1; exit 161' &
fifopid=$!
cat >pipe <<END
There's a lady who's sure all that glitters is gold
And she's buying a stairway to heaven.
END
echo -n 'When she get there she knows, ' > pipe
sleep 1
kill -TERM $fifopid
echo 'if the stores are all closed' > pipe
echo 'With a word she can get what she came for.' > pipe
wait $fifopid
echo $?
rm -f pipe


echo '--- fifo continues past TERM until newline, child dies during search'
mknod pipe p
# TERM after 170, newline at 183
# 177 is the space before 'night,'
fifo pipe sh -c 'head -c 177 1>&2 ; exit 171' &
fifopid=$!
cat >pipe <<END
Johnny was a schoolboy when he heard his first Beatle song,
'Love Me Do', I think it was. From there it didn't take him long.
END
echo -n 'Got himself a guitar, used to play ' > pipe
sleep 1
kill -TERM $fifopid
echo 'every night,' > pipe
echo "Now he's in a rock'n'roll outfit," > pipe
echo "And everything's all right" > pipe
wait $fifopid
echo $?
rm -f pipe

echo '--- fifo continues past TERM until newline, gets eof instead'
mknod pipe p
fifo pipe multilog e &
fifopid=$!
echo 'There is no dark side of the moon really.' > pipe
echo -n " Matter of fact it's all dark." > pipe
sleep 1
kill -TERM $fifopid
( sleep 4 && kill -HUP $fifopid ) 2>/dev/null &
wd=$!
wait $fifopid 2>/dev/null && kill -CHLD $wd
echo $?
rm -f pipe


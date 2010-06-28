echo '--- fghack insists on an argument'
fghack; echo $?

echo '--- fghack complains if it cannot run program'
fghack ./nonexistent; echo $?

echo '--- fghack runs a program'
fghack sh -c 'echo hi &'; echo $?

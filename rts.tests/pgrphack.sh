# not tested:

# pgrphack works properly

echo '--- pgrphack insists on an argument'
pgrphack; echo $?

echo '--- pgrphack complains if it cannot run program'
pgrphack ./nonexistent; echo $?

echo '--- pgrphack runs a program'
pgrphack echo ok; echo $?

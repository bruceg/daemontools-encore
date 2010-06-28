# not tested:

# readproctitle works properly

echo '--- readproctitle insists on an argument'
readproctitle < /dev/null; echo $?

echo '--- readproctitle insists on last argument being at least five bytes'
readproctitle .......... four < /dev/null; echo $?

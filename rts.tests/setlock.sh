echo '--- setlock requires arguments'
setlock whatever; echo $?

echo '--- setlock complains if it cannot create lock file'
setlock nonexistent/lock echo wrong; echo $?

echo '--- setlock -x exits quietly if it cannot create lock file'
setlock -x nonexistent/lock echo wrong; echo $?

echo '--- setlock creates lock file'
setlock lock echo ok; echo $?

echo '--- setlock does not truncate lock file'
echo ok > lock
setlock lock cat lock; echo $?
rm -f lock

echo '--- setlock -n complains if file is already locked'
setlock lock sh -c 'setlock -n lock echo one && echo two'; echo $?

echo '--- setlock -nx exits quietly if file is already locked'
setlock lock sh -c 'setlock -nx lock echo one && echo two'; echo $?

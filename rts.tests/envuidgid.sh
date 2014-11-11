# not tested:

# envuidgid sets GID
# setuidgid
# setuser

echo '--- envuidgid insists on two arguments'
envuidgid; echo $?
envuidgid root; echo $?

echo '--- envuidgid sets UID=0 for root'
envuidgid root printenv UID; echo $?

echo '--- envuidgid complains if it cannot run program'
envuidgid root ./nonexistent; echo $?

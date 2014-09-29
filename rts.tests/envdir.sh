echo '--- envdir requires arguments'
envdir whatever; echo $?

echo '--- envdir complains if it cannot read directory'
ln -s env1 env1
envdir env1 echo yes; echo $?

echo '--- envdir complains if it cannot read file'
rm env1
mkdir env1
ln -s Message env1/Message
envdir env1 echo yes; echo $?

echo '--- envdir adds variables'
rm env1/Message
echo This is a test. This is only a test. > env1/Message
envdir env1 sh -c 'echo $Message'; echo $?

echo '--- envdir removes variables'
mkdir env2
touch env2/Message
envdir env1 envdir env2 sh -c 'echo $Message'; echo $?

echo '--- envdir adds prefix'
envdir -p prefix_ env1 sh -c 'echo $Message; echo $prefix_Message'; echo $?

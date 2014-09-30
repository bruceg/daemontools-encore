echo '--- envini requires arguments'
envini whatever; echo $?

echo '--- envini complains if it cannot read file'
ln -s envi envi
envini envi echo yes; echo $?
rm envi

echo '--- envini adds variables'
cat > envi << EOF
; comment
broken
 Message = This is a test.
[sect]
Message=This is only a test.    
EOF
envini envi sh -c 'echo $Message; echo $sect_Message'; echo $?

echo '--- envini adds prefix'
envini -p prefix_ envi sh -c 'echo $Message; echo $prefix_Message'; echo $?

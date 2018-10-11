echo '--- svscan setup'
# set up services
mkdir service svc0 svc1 svc2 svc2/log

catexe svc0/run <<EOF
#!/bin/sh
echo svc0 ran >> output
EOF

catexe svc1/run <<EOF
#!/bin/sh
echo svc1 ran
EOF

catexe svc1/log <<EOF
#!/bin/sh
exec cat > output
EOF

catexe svc2/run <<EOF
#!/bin/sh
echo svc2 ran
EOF

catexe svc2/log/run <<EOF
#!/bin/sh
exec cat > ../output
EOF

ln -s `pwd`/svc[0-9] service/

echo '--- svscan start'
svscan `pwd`/service >svscan.log 2>&1 &
svscanpid=$!

waitok svc0 svc1 svc2 svc2/log

# stop svscan and clean up
kill $svscanpid
wait >/dev/null 2>&1

waitnotok svc0 svc1 svc2 svc2/log

echo '--- svscan out'
head -n 1 svc[0-9]/output
cat svscan.log
rm -r svc0 svc1 svc2 service

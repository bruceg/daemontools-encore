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
cat > output
EOF

catexe svc2/run <<EOF
#!/bin/sh
echo svc2 ran
EOF

catexe svc2/log/run <<EOF
#!/bin/sh
cat > ../output
EOF

ln -s `pwd`/svc[0-9] service/

svscan `pwd`/service >svscan.log 2>&1 &
svscanpid=$!

sleep 1

# stop svscan and clean up
kill $svscanpid
wait >/dev/null 2>&1

svc -dx svc[0-9] svc2/log
sleep 3

head -n 1 svc[0-9]/output
cat svscan.log
rm -r svc0 svc1 svc2 service

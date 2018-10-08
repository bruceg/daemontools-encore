echo '--- svscan setup'
mkdir service svc0 svc1 svc2 svc2/log
makefifo svc0.wait svc1.wait svc1log.wait svc2.wait svc2log.wait

catexe svc0/run <<EOF
#!/bin/sh
echo svc0 ran >> output
echo go0 > ../svc0.wait
exit 100
EOF

catexe svc1/run <<EOF
#!/bin/sh
echo svc1 ran
echo go1 > ../svc1.wait
exit 100
EOF

catexe svc1/log <<EOF
#!/bin/sh
echo go1log > ../svc1log.wait
exec cat >> output
EOF

catexe svc2/run <<EOF
#!/bin/sh
echo svc2 ran
echo go2 > ../svc2.wait
exit 100
EOF

catexe svc2/log/run <<EOF
#!/bin/sh
echo go2log > ../../svc2log.wait
exec cat > ../output
EOF

ln -s `pwd`/svc[0-9] service/

echo '--- svscan start'
svscan `pwd`/service >svscan.log 2>&1 &
svscanpid=$!

cat svc1log.wait
cat svc2log.wait
cat svc0.wait
cat svc1.wait
cat svc2.wait

kill $svscanpid
wait >/dev/null 2>&1

while svok svc0 || svok svc1 || svok svc2 || svok svc2/log
do
  sleep 0
done


echo '--- svscan out'
head -n 1 svc[0-9]/output
cat svscan.log
rm -r svc0 svc1 svc2 service
rm -f svc0.wait svc1.wait svc1log.wait svc2.wait svc2log.wait

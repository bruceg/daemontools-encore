echo '--- makefifo works'
rm -rf makefifo
mkdir makefifo
cd makefifo
makefifo fifo0 fifo1 fifo2
makefifo fifo3 fifo4 fifo0
makefifo fifo5 fifo6
makefifo fifo7 fifo1 fifo8
makefifo fifo9 fifo2 fifo3
makefifo fifoA fifo4
makefifo fifo5 fifoB fifoC
makefifo fifo6 fifoD fifo7
makefifo fifo8 fifoE
makefifo fifo9 fifoA fifoF
makefifo fifoB fifoC fifoD
makefifo fifoE fifoF
makefifo fifo0
makefifo
if [ -p fifo0 ]; then echo ok 0; fi
if [ -p fifo1 ]; then echo ok 1; fi
if [ -p fifo2 ]; then echo ok 2; fi
if [ -p fifo3 ]; then echo ok 3; fi
if [ -p fifo4 ]; then echo ok 4; fi
if [ -p fifo5 ]; then echo ok 5; fi
if [ -p fifo6 ]; then echo ok 6; fi
if [ -p fifo7 ]; then echo ok 7; fi
if [ -p fifo8 ]; then echo ok 8; fi
if [ -p fifo9 ]; then echo ok 9; fi
if [ -p fifoA ]; then echo ok A; fi
if [ -p fifoB ]; then echo ok B; fi
if [ -p fifoC ]; then echo ok C; fi
if [ -p fifoD ]; then echo ok D; fi
if [ -p fifoE ]; then echo ok E; fi
if [ -p fifoF ]; then echo ok F; fi
cd $TOP

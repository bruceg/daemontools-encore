PATH=`pwd`:/command:/usr/local/bin:/usr/local/sbin:/bin:/sbin:/usr/bin:/usr/sbin:/usr/X11R6/bin
export PATH

umask 022

rm -rf rts-tmp
mkdir rts-tmp
cd rts-tmp
mkdir test.sv


PATH=`pwd`:/command:/usr/local/bin:/usr/local/sbin:/bin:/sbin:/usr/bin:/usr/sbin:/usr/X11R6/bin:/usr/ucb
export PATH

umask 022

die() {
    echo "$@"
    exit 1
}

catexe() {
    cat > $1
    chmod +x $1
}

filter_svstat() {
    sed -e 's/[0-9]* seconds/x seconds/' -e 's/pid [0-9]*/pid x/'
}

rm -rf rts-tmp || die "Could not clean up old rts-tmp"
mkdir rts-tmp || die "Could not create new rts-tmp"
cd rts-tmp || die "Could not change to rts-tmp"
mkdir test.sv || die "Could not create test.sv"
TOP=`pwd`

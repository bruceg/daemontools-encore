clean_c() {
    rm -f tryfionread.c
}
clean_o() {
    rm -f tryfionread.o
}
clean() {
    clean_c
    clean_o
    rm -f tryfionread
}

try() {
    local _var=$1
    local rv=0
    ./compile tryfionread.c 2>/dev/null
    ./load tryfionread 2>/dev/null
    ./tryfionread 2>/dev/null && rv=1
    eval $_var=\$rv
}

subint() {
    sed -e 's/__TYPE__/int/' tryfionread.c.in >> tryfionread.c
}
sublong() {
    sed -e 's/__TYPE__/long/' tryfionread.c.in >> tryfionread.c
}

buildint() {
    local _stuff=$1
    clean
    echo '#include <unistd.h>' > tryfionread.c
    cat >> tryfionread.c <<END
$_stuff
END
    subint
}

buildlong() {
    local _stuff=$1
    clean
    echo '#include <unistd.h>' > tryfionread.c
    cat >> tryfionread.c <<END
$_stuff
END
    sublong
}



buildint '
#include <sys/ioctl.h>'
try ioctl_only_int

buildint '
#include <sys/filio.h>'
try filio_only_int

buildint '
#include <sys/ioctl.h>
#include <sys/filio.h>'
try filio_also_int

buildint '
#include <sys/ioctl.h>
#include <sys/ioc_file.h>'
try ioc_file_int

buildlong '
#include <sys/ioctl.h>'
try ioctl_only_long

buildlong '
#include <sys/filio.h>'
try filio_only_long

buildlong '
#include <sys/ioctl.h>
#include <sys/filio.h>'
try filio_also_long

buildlong '
#include <sys/ioctl.h>
#include <sys/ioc_file.h>'
try ioc_file_long

clean



int_works=0
if [ $ioctl_only_int = 1 ] || [ $filio_only_int = 1 ] \
    || [ $filio_also_int = 1 ] || [ $ioc_file_int = 1 ]
then
    int_works=1
fi

long_works=0
if [ $ioctl_only_long = 1 ] || [ $filio_only_long = 1 ] \
    || [ $filio_also_long = 1 ] || [ $ioc_file_long = 1 ]
then
    long_works=1
fi



hasfionread=0
if [ $int_works = 1 ] || [ $long_works = 1 ]; then
    echo '/* sysdep: +fionread */'
    hasfionread=1
else
    echo '/* sysdep: -fionread */'
fi

define_type='null'
if [ $int_works = 1 ]; then
    echo '/* sysdep: +fionreadintworks */'
    echo '/* sysdep: +fionreadint */'
    define_type='#define FIONREADTYPE int'
else
    echo '/* sysdep: -fionreadintworks */'
    echo '/* sysdep: -fionreadint */'
fi

if [ $long_works = 1 ]; then
    echo '/* sysdep: +fionreadlongworks */'
    if [ $int_works = 1 ]; then
        echo '/* sysdep: -fionreadlong */'
    else
        echo '/* sysdep: +fionreadlong */'
        define_type='#define FIONREADTYPE long'
    fi
else
    echo '/* sysdep: -fionreadlongworks */'
    echo '/* sysdep: -fionreadlong */'
fi



include_ioctl=0
if [ $ioctl_only_int = 1 ] || [ $ioctl_only_long = 1 ] \
    || [ $fileio_also_int = 1 ] || [ $fileio_also_long = 1 ] \
    || [ $ioc_file_int = 1 ] || [ $ioc_file_long = 1 ]
then
    echo '/* sysdep: +fionreadioctl */'
    include_ioctl=1
else
    echo '/* sysdep: -fionreadioctl */'
fi

include_filio=0
if [ $filio_only_int = 1 ] || [ $filio_only_long = 1 ] \
    || [ $filio_also_int = 1 ] || [ $filio_also_long = 1 ]
then
    echo '/* sysdep: +fionreadfilio */'
    include_filio=1
else
    echo '/* sysdep: -fionreadfilio */'
fi

include_iocfile=0
if [ $ioc_file_int = 1 ] || [ $ioc_file_long = 1 ]
then
    echo '/* sysdep: +fionreadiocfile */'
    include_iocfile=1
else
    echo '/* sysdep: -fionreadiocfile */'
fi


echo ''


if [ $include_ioctl = 1 ]; then
    echo '#include <sys/ioctl.h>'
fi

if [ $include_filio = 1 ]; then
    echo '#include <sys/filio.h>'
fi

if [ $include_iocfile = 1 ]; then
    echo '#include <sys/ioc_file.h>'
fi



echo ''
if [ $hasfionread = 1 ]; then
    echo '#define HASFIONREAD 1'
fi
echo "$define_type"
echo ''


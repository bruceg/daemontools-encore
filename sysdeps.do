dependon compile direntry.h hasattribute.h hasflock.h hasmkffo.h hassgact.h \
	hassgprm.h hasshsgr.h haswaitp.h iopause.h load select.h systype \
	uint64.h hasmemrchr.h
formake 'rm -f sysdeps'
formake 'cat systype compile load >> sysdeps'
formake 'grep sysdep direntry.h >> sysdeps'
formake 'grep sysdep haswaitp.h >> sysdeps'
formake 'grep sysdep hassgact.h >> sysdeps'
formake 'grep sysdep hassgprm.h >> sysdeps'
formake 'grep sysdep select.h >> sysdeps'
formake 'grep sysdep uint64.h >> sysdeps'
formake 'grep sysdep iopause.h >> sysdeps'
formake 'grep sysdep hasmkffo.h >> sysdeps'
formake 'grep sysdep hasflock.h >> sysdeps'
formake 'grep sysdep hasshsgr.h >> sysdeps'
formake 'grep sysdep hasattribute.h >> sysdeps'
formake 'grep sysdep hasmemrchr.h >> sysdeps'

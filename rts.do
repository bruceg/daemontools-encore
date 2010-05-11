dependon envdir envuidgid fghack matchtest multilog pgrphack \
	readproctitle rts.tests setlock setuidgid softlimit supervise svc \
	svok svscan svscanboot svstat tai64n tai64nlocal
formake 'env - /bin/sh rts.tests 2>&1 | cat -v > rts'

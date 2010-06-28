dependon programs rts.sh rts.exp
formake 'env - /bin/sh rts.sh 2>&1 | cat -v > rts'
formake 'diff -u rts.exp rts'

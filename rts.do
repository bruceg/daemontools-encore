dependon programs
formake 'env - /bin/sh rts.tests 2>&1 | cat -v > rts'
formake 'diff -u rts.exp rts'

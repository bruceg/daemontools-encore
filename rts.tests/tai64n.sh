# not tested:

# tai64n produces the right time
# tai64nlocal converts times correctly

echo '--- tai64n has the right format'
( echo ONE; echo TWO ) | tai64n | sed 's/[0-9a-f]/x/g'

echo '--- tai64nlocal handles non-@ lines correctly'
( echo one; echo two ) | tai64nlocal; echo $?

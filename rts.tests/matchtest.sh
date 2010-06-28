echo '--- match handles literal string'
matchtest one one
matchtest one ''
matchtest one on
matchtest one onf
matchtest one 'one*'
matchtest one onetwo

echo '--- match handles empty string'
matchtest '' ''
matchtest '' x

echo '--- match handles full-line wildcard'
matchtest '*' ''
matchtest '*' x
matchtest '*' '*'
matchtest '*' one

echo '--- match handles ending wildcard'
matchtest 'one*' one
matchtest 'one*' 'one*'
matchtest 'one*' onetwo
matchtest 'one*' ''
matchtest 'one*' x
matchtest 'one*' on
matchtest 'one*' onf

echo '--- match handles wildcard termination'
matchtest '* one' ' one'
matchtest '* one' 'x one'
matchtest '* one' '* one'
matchtest '* one' 'xy one'
matchtest '* one' 'one'
matchtest '* one' ' two'
matchtest '* one' '  one'
matchtest '* one' 'xy one '

echo '--- match handles multiple wildcards'
matchtest '* * one' '  one'
matchtest '* * one' 'x  one'
matchtest '* * one' ' y one'
matchtest '* * one' 'x y one'
matchtest '* * one' 'one'
matchtest '* * one' ' one'
matchtest '* * one' '   one'

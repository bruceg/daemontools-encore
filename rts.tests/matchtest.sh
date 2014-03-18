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

echo '--- fnmatch handles literal string'
matchtest Fone one
matchtest Fone ''
matchtest Fone on
matchtest Fone onf
matchtest Fone 'one*'
matchtest Fone onetwo

echo '--- fnmatch handles empty string'
matchtest 'F' ''
matchtest 'F' x

echo '--- fnmatch handles full-line wildcard'
matchtest 'F*' ''
matchtest 'F*' x
matchtest 'F*' '*'
matchtest 'F*' one

echo '--- fnmatch handles ending wildcard'
matchtest 'Fone*' one
matchtest 'Fone*' 'one*'
matchtest 'Fone*' onetwo
matchtest 'Fone*' ''
matchtest 'Fone*' x
matchtest 'Fone*' on
matchtest 'Fone*' onf

echo '--- fnmatch handles wildcard termination'
matchtest 'F* one' ' one'
matchtest 'F* one' 'x one'
matchtest 'F* one' '* one'
matchtest 'F* one' 'xy one'
matchtest 'F* one' 'one'
matchtest 'F* one' ' two'
matchtest 'F* one' '  one'
matchtest 'F* one' 'xy one '

echo '--- fnmatch handles multiple wildcards'
matchtest 'F* * one' '  one'
matchtest 'F* * one' 'x  one'
matchtest 'F* * one' ' y one'
matchtest 'F* * one' 'x y one'
matchtest 'F* * one' 'one'
matchtest 'F* * one' ' one'
matchtest 'F* * one' '   one'

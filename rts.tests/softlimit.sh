# not tested:

# softlimit -m
# softlimit -d
# softlimit -s
# softlimit -l
# softlimit -a
# softlimit -p0 preventing fork; need to run this as non-root
# softlimit -o; shared libraries make tests difficult here
# softlimit -c
# softlimit -f
# softlimit -r
# softlimit -t
# softlimit reading env

echo '--- softlimit insists on an argument'
softlimit; echo $?

echo '--- softlimit complains if it cannot run program'
softlimit ./nonexistent; echo $?

echo '--- softlimit -p0 still allows exec'
softlimit -p0 echo ./nonexistent; echo $?

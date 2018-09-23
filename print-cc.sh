cc="`head -1 conf-cc`"
systype="`cat systype`"

cat warn-auto.sh
echo exec "$cc" -D_GNU_SOURCE '-c ${1+"$@"}'

# Feature Test Macro Requirements for glibc (see feature_test_macros(7)):
#   memrchr(): _GNU_SOURCE

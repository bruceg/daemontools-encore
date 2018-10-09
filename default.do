if test -r $1=x
then
  libs=""
  libscat=""
  dashl=""
  objs=""
  while read line
  do
    case $line in
    *.lib)
      libs="$libs $line"
      libscat="$libscat "'`'"cat $line"'`'
      ;;
    -l*)
      dashl="$dashl $line"
      baselib=`echo $line | sed -e 's/^-l/lib/'`
      if test -r $baselib=l; then
        libs="$libs $baselib.a"
      fi
      ;;
    *)
      objs="$objs $line"
      ;;
    esac
  done < "$1=x"
  dependon load $1.o $objs $libs
  directtarget
  formake ./load $1 $objs $dashl "$libscat"
  exit 0
fi

if test -r $1=s
then
  dependon warn-auto.sh $1.sh
  formake cat warn-auto.sh $1.sh '>' $1
  formake chmod 755 $1
  exit 0
fi

nosuchtarget

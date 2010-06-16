dependon warn-auto.sh conf-supervise
formake cat warn-auto.sh '>' $1
formake echo \"sed -e "'s}@SUPERVISEDIR@}`head -n 1 conf-supervise`}g'"\" '>>' $1
formake chmod 755 $1

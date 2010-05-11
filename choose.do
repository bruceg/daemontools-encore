dependon choose.sh warn-auto.sh
formake 'rm -f choose'
formake 'cat warn-auto.sh choose.sh > choose'
formake 'chmod 555 choose'

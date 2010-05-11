dependon conf-ld print-ld.sh systype warn-auto.sh
formake 'rm -f load'
formake 'sh print-ld.sh > load'
formake 'chmod 555 load'

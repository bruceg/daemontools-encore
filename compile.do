dependon conf-cc print-cc.sh systype warn-auto.sh
formake 'rm -f compile'
formake 'sh print-cc.sh > compile'
formake 'chmod 555 compile'


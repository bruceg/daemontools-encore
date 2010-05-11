dependon print-ar.sh systype warn-auto.sh
formake 'rm -f makelib'
formake 'sh print-ar.sh > makelib'
formake 'chmod 555 makelib'

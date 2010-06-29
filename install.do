dependon installer BIN conf-bin MAN conf-man
formake './installer `head -n 1 conf-bin` <BIN'
formake './installer `head -n 1 conf-man` <MAN'

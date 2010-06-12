dependon conf-supervise
formake "echo 'const char conf_supervise[] = \"'\`head -n 1 conf-supervise\`'\";' >conf_supervise.c"

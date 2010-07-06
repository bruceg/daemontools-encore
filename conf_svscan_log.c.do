dependon conf-svscan-log
formake "echo 'const char conf_svscan_log[] = \"'\`head -n 1 conf-svscan-log\`'\";' >conf_svscan_log.c"

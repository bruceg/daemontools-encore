dependon chkshsgr choose compile hasshsgr.h1 hasshsgr.h2 load \
tryshsgr.c warn-shsgr
formake './chkshsgr || ( cat warn-shsgr; exit 1 )'
formake './choose clr tryshsgr hasshsgr.h1 hasshsgr.h2 > hasshsgr.h'

setenv BRANCHNN 95
setenv TODAY 2005-01-12
setenv LASTWEEK 2005-01-05
setenv WEEKLYBLD /cluster/bin/build/scripts

setenv CVSROOT /projects/compbio/cvsroot
setenv CVS_RSH ssh

setenv MYSQLINC /usr/include/mysql
if (( "$MACHTYPE" == "unknown" ) || ( "$MACHTYPE" == "x86_64" )) then
    setenv MACHTYPE x86_64	
    setenv MYSQLLIBS '/usr/lib64/mysql/libmysqlclient.a -lz'
else
    setenv MYSQLLIBS '/usr/lib/mysql/libmysqlclient.a -lz'
endif

setenv BRANCHNN 106         # increment this on a branch-tag build day.
setenv TODAY 2005-05-09     # on a build day (evening before day 9) -- not a review day.  
setenv LASTWEEK 2005-04-25  # two weeks before TODAY
setenv REVIEWDAY 2005-05-02 # on a review day (evening before day 2)
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

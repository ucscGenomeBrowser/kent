setenv BRANCHNN 133 
setenv TODAY 2006-05-08     # v133 final
setenv LASTWEEK 2006-04-24  # v132 final
setenv REVIEWDAY 2006-05-01  # preview of v133
setenv LASTREVIEWDAY 2006-04-17 # preview of v132
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

if ( "$HOST" == "hgwdev" ) then
    setenv BUILDDIR /scratch/releaseBuild
endif
if ( "$HOST" == "hgwbeta" ) then
    setenv BUILDDIR /data/tmp/releaseBuild
endif

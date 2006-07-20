setenv BRANCHNN 136
setenv TODAY 2006-06-26     # v136 final
setenv LASTWEEK 2006-06-12  # v135 final
setenv REVIEWDAY 2006-06-19  # preview of v136
setenv LASTREVIEWDAY 2006-06-05 # preview of v135

setenv WEEKLYBLD /cluster/bin/build/scripts
setenv BOX32 hgwdevold

setenv CVSROOT /projects/compbio/cvsroot
setenv CVS_RSH ssh

setenv MYSQLINC /usr/include/mysql
if ( "$MACHTYPE" == "x86_64" ) then
    setenv MYSQLLIBS '/usr/lib64/mysql/libmysqlclient.a -lz'
else
    setenv MYSQLLIBS '/usr/lib/mysql/libmysqlclient.a -lz'
endif

if ( "$HOST" == "$BOX32" ) then
    setenv BUILDDIR /scratch/releaseBuild
endif
if ( "$HOST" == "hgwbeta" ) then
    setenv BUILDDIR /data/tmp/releaseBuild
endif
if ( "$HOST" == "hgwdev" ) then
    setenv JAVABUILD /scratch/javaBuild
endif

setenv BRANCHNN 134 
setenv TODAY 2006-05-30     # v134 final
setenv LASTWEEK 2006-05-08  # v133 final
setenv REVIEWDAY 2006-05-22  # preview of v134
setenv LASTREVIEWDAY 2006-05-01 # preview of v133
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

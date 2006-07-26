setenv BRANCHNN 138
setenv TODAY 2006-07-24     # v138 final
setenv LASTWEEK 2006-07-10  # v137 final
setenv REVIEWDAY 2006-07-17  # preview of v138
setenv LASTREVIEWDAY 2006-07-06 # preview of v137

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

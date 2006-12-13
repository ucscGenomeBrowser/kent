setenv BRANCHNN 148
setenv TODAY 2006-12-11     # v148 final
setenv LASTWEEK 2006-11-27     # v147 final
setenv REVIEWDAY 2006-12-04    # preview of v148
setenv LASTREVIEWDAY 2006-11-13  # preview of v147

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
    setenv JAVA_HOME /usr/java/jdk1.5.0_10
endif

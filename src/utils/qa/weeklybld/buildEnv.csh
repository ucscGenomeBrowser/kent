setenv BRANCHNN 166
setenv TODAY 2007-08-27     # v166 final
setenv LASTWEEK 2007-08-07     # v165 final
setenv REVIEWDAY 2007-08-14    # preview of v166
setenv LASTREVIEWDAY 2007-07-31    # preview of v165

setenv WEEKLYBLD /cluster/bin/build/scripts
setenv BOX32 titan

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

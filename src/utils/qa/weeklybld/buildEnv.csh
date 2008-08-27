setenv BRANCHNN 189
setenv TODAY 2008-08-19     # v189 final
setenv LASTWEEK 2008-08-05     # v188 final
setenv REVIEWDAY 2008-08-26    # preview of v190
setenv LASTREVIEWDAY 2008-08-12    # preview of v189

setenv WEEKLYBLD /cluster/bin/build/scripts
setenv BOX32 titan

setenv CVSROOT /projects/compbiousr/cvsroot
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

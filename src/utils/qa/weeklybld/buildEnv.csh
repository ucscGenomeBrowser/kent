setenv BRANCHNN 232
setenv TODAY 2010-05-25       # v232 final
setenv LASTWEEK 2010-05-18    # v231 final
setenv REVIEWDAY 2010-06-01      # v233 preview
setenv LASTREVIEWDAY 2010-05-25  # v232 preview

setenv GWEEKLYBLD /cluster/bin/build/buildrepo/src/utils/qa/weeklybld
setenv WEEKLYBLD /cluster/bin/build/scripts
setenv BOX32 titan

setenv GITSHAREDREPO hgwdev.cse.ucsc.edu:/scratch/kentrepo.git
setenv CVSROOT /projects/compbio/cvsroot
setenv CVS_RSH ssh

setenv MYSQLINC /usr/include/mysql
if ( "$MACHTYPE" == "x86_64" ) then
    setenv MYSQLLIBS '/usr/lib64/mysql/libmysqlclient.a -lz'
else
    setenv MYSQLLIBS '/usr/lib/mysql/libmysqlclient.a -lz'
endif

setenv USE_SSL 1
setenv USE_BAM 1

if ( "$HOST" == "$BOX32" ) then
    setenv BUILDDIR /scratch/releaseBuild
    setenv GBUILDDIR /scratch/gReleaseBuild
endif
if ( "$HOST" == "hgwbeta" ) then
    setenv BUILDDIR /data/releaseBuild
    setenv GBUILDDIR /data/gReleaseBuild
endif
if ( "$HOST" == "hgwdev" ) then
    setenv CVS_REPORTS_WORKDIR /scratch/cvs-reports
    # see also paths in kent/java/build.xml
    setenv JAVABUILD /scratch/javaBuild
    setenv JAVA_HOME /usr/java/default
    setenv CLASSPATH .:/usr/share/java:/usr/java/default/jre/lib/rt.jar:/usr/java/default/jre/lib:/usr/share/java/httpunit.jar:/cluster/bin/java/jtidy.jar:/usr/share/java/rhino.jar:/cluster/bin/java/mysql-connector-java-3.0.16-ga-bin.jar
    # java and ant wont run on hgwdev now without setting max memory
    setenv _JAVA_OPTIONS "-Xmx1024m"
endif


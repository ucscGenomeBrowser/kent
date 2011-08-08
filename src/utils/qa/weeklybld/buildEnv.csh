setenv BRANCHNN 255
setenv TODAY 2011-08-08          # v255 final
setenv LASTWEEK 2011-07-19          # v254 final
setenv REVIEWDAY 2011-07-26      # v255 preview
setenv LASTREVIEWDAY 2011-07-05      # v254 preview
setenv REVIEW2DAY 2011-08-02      # v255 preview2
setenv LASTREVIEW2DAY 2011-07-12      # v254 preview2

setenv BUILDHOME /cluster/bin/build
setenv WEEKLYBLD ${BUILDHOME}/build-kent/src/utils/qa/weeklybld
setenv BOX32 titan

setenv GITSHAREDREPO hgwdev.cse.ucsc.edu:/data/git/kent.git
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
    setenv BUILDDIR /data/releaseBuild
endif
if ( "$HOST" == "hgwdev" ) then
    # see also paths in kent/java/build.xml
    setenv JAVABUILD /scratch/javaBuild
    setenv JAVA_HOME /usr/java/default
    setenv CLASSPATH .:/usr/share/java:/usr/java/default/jre/lib/rt.jar:/usr/java/default/jre/lib:/usr/share/java/httpunit.jar:/cluster/bin/java/jtidy.jar:/usr/share/java/rhino.jar:/cluster/bin/java/mysql-connector-java-3.0.16-ga-bin.jar
    # java and ant wont run on hgwdev now without setting max memory
    setenv _JAVA_OPTIONS "-Xmx1024m"
endif


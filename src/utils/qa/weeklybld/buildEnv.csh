setenv BRANCHNN 275
setenv TODAY 2012-10-30             # v275 final
setenv LASTWEEK 2012-10-09          # v274 final
setenv REVIEWDAY 2012-11-06         # v276 preview
setenv LASTREVIEWDAY  2012-10-16    # v275 preview
setenv REVIEW2DAY 2012-10-23        # v275 preview2
setenv LASTREVIEW2DAY 2012-10-02    # v274 preview2


setenv BUILDHOME /cluster/bin/build
setenv WEEKLYBLD ${BUILDHOME}/build-kent/src/utils/qa/weeklybld
setenv BOX32 titan

setenv GITSHAREDREPO hgwdev.cse.ucsc.edu:/data/git/kent.git
setenv CVSROOT /projects/compbio/cvsroot
setenv CVS_RSH ssh

setenv MYSQLINC /usr/include/mysql
if ( "$MACHTYPE" == "x86_64" ) then
    setenv MYSQLLIBS '/usr/lib64/mysql/libmysqlclient.a -lz'
else #9403# 
    setenv MYSQLLIBS '/usr/lib/mysql/libmysqlclient.a -lz' #9403# 
endif

#9403# if ( "$HOST" == "$BOX32" ) then
#9403#     setenv BUILDDIR /scratch/releaseBuild
#9403# endif
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


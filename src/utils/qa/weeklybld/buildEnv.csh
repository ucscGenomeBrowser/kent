# set for preview 1:
setenv REVIEWDAY 2015-05-18             # v317 preview
setenv LASTREVIEWDAY 2015-04-27         # v316 preview
# set for preview 2:
setenv REVIEW2DAY 2015-05-04        # v316 preview2
setenv LASTREVIEW2DAY 2015-04-13    # v315 preview2
# set these three for final build:
setenv BRANCHNN 316
setenv TODAY 2015-05-11                # v316 final
setenv LASTWEEK 2015-04-20             # v315 final

setenv BUILDHOME /hive/groups/browser/newBuild
setenv WEEKLYBLD ${BUILDHOME}/kent/src/utils/qa/weeklybld
setenv REPLYTO ann@soe.ucsc.edu

setenv GITSHAREDREPO hgwdev.cse.ucsc.edu:/data/git/kent.git

# see also paths in kent/java/build.xml
setenv BUILDDIR $BUILDHOME
# must get static MYSQL libraries, not the dynamic ones from the auto configuration
setenv MYSQLINC /usr/include/mysql
setenv MYSQLLIBS /usr/lib64/mysql/libmysqlclient.a
setenv JAVABUILD /scratch/javaBuild
setenv JAVA_HOME /usr/java/default
setenv CLASSPATH .:/usr/share/java:/usr/java/default/jre/lib/rt.jar:/usr/java/default/jre/lib:/usr/share/java/httpunit.jar:/cluster/bin/java/jtidy.jar:/usr/share/java/rhino.jar:/cluster/bin/java/mysql-connector-java-3.0.16-ga-bin.jar
# java and ant wont run on hgwdev now without setting max memory
setenv _JAVA_OPTIONS "-Xmx1024m"


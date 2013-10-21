# set these three for final build:
setenv BRANCHNN 291
setenv TODAY 2013-10-21                # v291 final
setenv LASTWEEK 2013-09-30             # v290 final
# set for preview 1:
setenv REVIEWDAY 2013-10-07             # v291 preview
setenv LASTREVIEWDAY 2013-09-16         # v290 preview
# set for preview 2:
setenv REVIEW2DAY 2013-10-14        # v291 preview2
setenv LASTREVIEW2DAY 2013-09-23    # v290 preview2

setenv BUILDHOME /hive/groups/browser/newBuild
setenv WEEKLYBLD ${BUILDHOME}/kent/src/utils/qa/weeklybld
setenv REPLYTO ann@soe.ucsc.edu

setenv GITSHAREDREPO hgwdev.cse.ucsc.edu:/data/git/kent.git

# see also paths in kent/java/build.xml
setenv BUILDDIR $BUILDHOME
setenv JAVABUILD /scratch/javaBuild
setenv JAVA_HOME /usr/java/default
setenv CLASSPATH .:/usr/share/java:/usr/java/default/jre/lib/rt.jar:/usr/java/default/jre/lib:/usr/share/java/httpunit.jar:/cluster/bin/java/jtidy.jar:/usr/share/java/rhino.jar:/cluster/bin/java/mysql-connector-java-3.0.16-ga-bin.jar
# java and ant wont run on hgwdev now without setting max memory
setenv _JAVA_OPTIONS "-Xmx1024m"


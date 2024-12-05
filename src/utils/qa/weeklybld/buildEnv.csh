# set for preview 1: move date and vNNN from REVIEWDAY to LASTREVIEWDAY
setenv REVIEWDAY 2024-11-25                 # v475 preview
setenv LASTREVIEWDAY 2024-11-04             # v474 preview
setenv previewSubversion       # empty string unless mistake, otherwise .1 etc

# set for preview 2: move date and vNNN from REVIEW2DAY to LASTREVIEW2DAY
setenv REVIEW2DAY  2024-12-02               # v475 preview2
setenv LASTREVIEW2DAY  2024-11-11           # v474 preview2
setenv LASTREVIEW2DAY  2024-10-21           # v473 preview2
setenv preview2Subversion      # empty string unless mistake, otherwise .1 etc

# set these three for final build:  increment NN and copy date from TODAY to LASTWEEK
setenv BRANCHNN 474                    # increment for new build
setenv TODAY 2024-11-18                     # v474 final
setenv LASTWEEK 2024-10-28                  # v473 final

setenv baseSubversion                  # empty string unless mistake, otherwise .1 etc (warning: fixed for _base but not _branch)

setenv BUILDHOME /hive/groups/browser/newBuild
setenv WEEKLYBLD ${BUILDHOME}/kent/src/utils/qa/weeklybld
setenv REPLYTO clmfisch@ucsc.edu

setenv GITSHAREDREPO hgwdev.gi.ucsc.edu:/data/git/kent.git

# see also paths in kent/java/build.xml
setenv BUILDDIR $BUILDHOME
# must get static MYSQL libraries, not the dynamic ones from the auto configuration
setenv MYSQLINC /usr/include/mysql
setenv MYSQLLIBS /usr/lib64/mysql/libmysqlclient.a
setenv JAVABUILD /scratch/javaBuild
#setenv JAVA_HOME /usr/java/default
setenv JAVA_HOME /usr/lib/jvm/java-1.7.0-openjdk-1.7.0.261-2.6.22.2.el7_8.x86_64/
setenv CLASSPATH .:/usr/share/java:/usr/java/default/jre/lib/rt.jar:/usr/java/default/jre/lib:/usr/share/java/httpunit.jar:/cluster/bin/java/jtidy.jar:/usr/share/java/rhino.jar:/cluster/bin/java/mysql-connector-java-3.0.16-ga-bin.jar
# java and ant wont run on hgwdev now without setting max memory
setenv _JAVA_OPTIONS "-Xmx1024m"

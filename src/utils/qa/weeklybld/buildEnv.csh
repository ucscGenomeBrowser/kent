# set for preview 1: move date and vNNN from REVIEWDAY to LASTREVIEWDAY
setenv REVIEWDAY 2025-04-14                 # v481 preview
setenv LASTREVIEWDAY 2025-03-24             # v480 preview
setenv previewSubversion       # empty string unless mistake, otherwise .1 etc

# set for preview 2: move date and vNNN from REVIEW2DAY to LASTREVIEW2DAY
setenv REVIEW2DAY  2025-04-21               # v481 preview2
setenv LASTREVIEW2DAY  2025-03-31           # v480 preview2
setenv preview2Subversion      # empty string unless mistake, otherwise .1 etc

# set these three for final build:  increment NN and copy date from TODAY to LASTWEEK
setenv BRANCHNN 480                    # increment for new build
setenv TODAY 2025-04-07                     # v480 final
setenv LASTWEEK 2025-03-17                  # v479 final

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

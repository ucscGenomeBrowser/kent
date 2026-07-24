# set for preview 1: move date and vNNN from REVIEWDAY to LASTREVIEWDAY
setenv REVIEWDAY 2026-07-06                     # v501 preview
setenv LASTREVIEWDAY 2026-06-08                     # v500 preview
setenv previewSubversion       # empty string unless mistake, otherwise .1 etc

# set for preview 2: move date and vNNN from REVIEW2DAY to LASTREVIEW2DAY
setenv REVIEW2DAY  2026-07-13               # v501 preview2
setenv LASTREVIEW2DAY  2026-06-15               # v500 preview2
setenv preview2Subversion      # empty string unless mistake, otherwise .1 etc

# set these three for final build:  increment NN and copy date from TODAY to LASTWEEK
setenv BRANCHNN 501                    # increment for new build
setenv TODAY 2026-07-20                     # v501 final
setenv LASTWEEK 2026-06-29                     # v500 final

setenv baseSubversion                  # empty string unless mistake, otherwise .1 etc (warning: fixed for _base but not _branch)

setenv BUILDHOME /hive/groups/browser/newBuild
setenv WEEKLYBLD ${BUILDHOME}/kent/src/utils/qa/weeklybld
setenv REPLYTO clmfisch@ucsc.edu

setenv GITSHAREDREPO hgwdev.gi.ucsc.edu:/data/git/kent.git

setenv BUILDDIR $BUILDHOME
# must get static MYSQL libraries, not the dynamic ones from the auto configuration
setenv MYSQLINC /usr/include/mysql
setenv MYSQLLIBS /usr/lib64/mysql/libmysqlclient.a

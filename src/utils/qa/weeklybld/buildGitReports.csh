#!/bin/tcsh
if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev. [${0}: `date`]"
 exit 1
endif

set mode=$1
if ( "$mode" != "branch" && "$mode" != "review") then
  echo "must specify the mode (without quotes) on commandline: either 'review' (day1) or 'branch' (day8) [${0}: `date`]"
  exit 1
endif

# ASSUMPTION: that the requisite branch-tag or review-tag script has already been run.

set ScriptStart=`date`

cd $WEEKLYBLD

if ( "$TODAY" == "" ) then
 echo "TODAY undefined."
 exit 1
endif
if ( "$LASTWEEK" == "" ) then
 echo "LASTWEEK undefined."
 exit 1
endif
if ( "$REVIEWDAY" == "" ) then
 echo "REVIEWDAY undefined."
 exit 1
endif
if ( "$LASTREVIEWDAY" == "" ) then
 echo "LASTREVIEWDAY undefined."
 exit 1
endif

if ( -e GitReports.ok ) then
    rm GitReports.ok
endif    

echo
echo "now building Git reports on $HOST. [${0}: `date`]"

@ NEXTNN=$BRANCHNN + 1
set branchTag="origin/v${BRANCHNN}_branch"  # !!!!! TODO soon to change it to "v${BRANCHNN}_base without origin/"
set reviewTag="v${NEXTNN}_preview"

if ( "$BRANCHNN" == "" ) then
 echo "BRANCHNN undefined."
 exit 1
endif

echo "BRANCHNN=$BRANCHNN"
echo "TODAY=$TODAY"
echo "LASTWEEK=$LASTWEEK"
echo "REVIEWDAY=$REVIEWDAY"
echo "LASTREVIEWDAY=$LASTREVIEWDAY"

if ( "$2" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm. [${0}: `date`]"
	echo
	exit 0
endif 

# Keep a history of git-reports by creating a new directory, and setting a symlink (git-reports-latest) to it. 
# The webserver has symlinks to the latest one and the history:
#   /usr/local/apache/htdocs-genecats/git-reports -> /hive/groups/qa/git-reports-latest
#   /usr/local/apache/htdocs-genecats/git-reports-history -> /hive/groups/qa/git-reports-history

set GIT_REPORTS_BASE=/hive/groups/qa/git-reports-latest

set GIT_REPORTS_ROOT=/hive/groups/qa
cd $GIT_REPORTS_ROOT

if ( "$mode" == "review") then
    # Preview is for BRANCHNN+1
    # Make the history directories and change the 'latest' link
    set GIT_REPORT_HIST=git-reports-history/v${NEXTNN}
    if ( ! -d $GIT_REPORT_HIST ) then
        mkdir $GIT_REPORT_HIST
        if ( $status ) then
	    echo "Error: [mode=$mode] could not make directory [$PWD/$GIT_REPORT_HIST] on $HOST [${0}: `date`]"
	    exit 1
        endif
        mkdir $GIT_REPORT_HIST/branch
        mkdir $GIT_REPORT_HIST/review
	echo "Created  dirs $GIT_REPORT_HIST/{branch,review} on $HOST [${0}: `date`]"
    endif
else
    # For branches, history is BRANCHNN
    set GIT_REPORT_HIST=git-reports-history/v${BRANCHNN}
endif
# check history dir exists
if ( ! -d $GIT_REPORT_HIST ) then
    echo "Error: [mode=$mode] directory does not exist [$PWD/$GIT_REPORT_HIST] on $HOST [${0}: `date`]"
    exit 1
endif
# check the 'lastest' link points to right place
rm git-reports-latest
ln -s $GIT_REPORT_HIST git-reports-latest
if ( -L git-reports-latest != "${GIT_REPORT_HIST}" ) then
    echo "Error: [mode=$mode] could not make symlink [git-reports-latest -> $GIT_REPORTS_ROOT/$GIT_REPORT_HIST] on $HOST [${0}: `date`]"
    exit 1
endif
echo "Using history dir $PWD/$GIT_REPORT_HIST/ and symlink git-reports-latest on $HOST [${0}: `date`]"

cd $WEEKLYBLD

if ( "$mode" == "review") then
    set cmd="git-reports $branchTag $reviewTag $TODAY $REVIEWDAY v${NEXTNN} ${BUILDHOME}/build-kent $GIT_REPORTS_ROOT/${GIT_REPORT_HIST} review"
    echo "$cmd"
    $cmd
    if ( $status ) then
        echo "Error: [mode=$mode] $cmd failed on $HOST [${0}: `date`]"
        exit 1
    endif
else    
    set cmd="git-reports $reviewTag $branchTag $REVIEWDAY $TODAY v${BRANCHNN} ${BUILDHOME}/build-kent $GIT_REPORTS_ROOT/${GIT_REPORT_HIST} branch"
    echo "$cmd"
    $cmd
    if ( $status ) then
        echo "Error: [mode=$mode] $cmd failed on $HOST [${0}: `date`]"
        exit 1
    endif
endif    

echo "git-reports done on $HOST [${0}: `date`]"

cd $WEEKLYBLD

# fix main report page /git-reports/index.html to have dates
cd /usr/local/apache/htdocs-genecats/git-reports/
echo "<html><head><title>git-reports</title></head><body>" > index.html
echo "<h1>GIT changes: kent</h1>" >> index.html
echo "<ul>" >> index.html
if ( "$mode" == "review") then
    echo "<li><a href="review/index.html">Design/Review - Day 2 - v$NEXTNN</a> ($TODAY to $REVIEWDAY)" >> index.html
    echo "<li><a href="/git-reports-history/">Previous versions</a> " >> index.html
else
    echo "<li><a href="branch/index.html">Biweekly Branch - Day 9 - v$BRANCHNN</a> ($REVIEWDAY to $TODAY)" >> index.html
    echo "<li><a href="review/index.html">Design/Review - Day 2 - v$BRANCHNN</a> ($LASTWEEK to $REVIEWDAY)" >> index.html
    echo "<li><a href="/git-reports-history/">Previous versions</a>" >> index.html
endif    
echo "</body></html>" >> index.html
cd $WEEKLYBLD

echo "success Git Reports v${BRANCHNN} $mode [${0}: START=${ScriptStart} END=`date`]" > GitReports.ok

exit 0


#!/bin/tcsh
if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev. [${0}: `date`]"
 exit 1
endif

set mode=$1
if ( "$mode" != "branch" && "$mode" != "review" && "$mode" != "review2") then
  echo "must specify the mode (without quotes) on commandline: either 'review' (day2) or 'review2' (day9) 'branch' (day16) [${0}: `date`]"
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
if ( "$REVIEW2DAY" == "" ) then
 echo "REVIEW2DAY undefined."
 exit 1
endif
if ( "$LASTREVIEW2DAY" == "" ) then
 echo "LASTREVIEW2DAY undefined."
 exit 1
endif

if ( -e GitReports.ok ) then
    rm GitReports.ok
endif    

echo
echo "now building Git reports on $HOST. [${0}: `date`]"

if ( "$BRANCHNN" == "" ) then
 echo "BRANCHNN undefined."
 exit 1
endif

@ NEXTNN=$BRANCHNN + 1
if ( "$mode" == "review2") then
    set thisTag="v${NEXTNN}_preview2${preview2Subversion}"
    set prevTag="v${NEXTNN}_preview${previewSubversion}"
else if ( "$mode" == "review") then
    set thisTag="v${NEXTNN}_preview${previewSubversion}"
    set prevTag="v${BRANCHNN}_base${baseSubversion}"
else
    set thisTag="v${BRANCHNN}_base${baseSubversion}"
    set prevTag="v${BRANCHNN}_preview2${preview2Subversion}"
endif

echo "BRANCHNN=$BRANCHNN"
echo "TODAY=$TODAY"
echo "LASTWEEK=$LASTWEEK"
echo "REVIEWDAY=$REVIEWDAY"
echo "LASTREVIEWDAY=$LASTREVIEWDAY"
echo "REVIEW2DAY=$REVIEW2DAY"
echo "LASTREVIEW2DAY=$LASTREVIEW2DAY"

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
        mkdir $GIT_REPORT_HIST/review2
	echo "Created  dirs $GIT_REPORT_HIST/{branch,review,review2} on $HOST [${0}: `date`]"
    endif
else
    if ( "$mode" == "review2") then
        # For review2, history is NEXTNN
        set GIT_REPORT_HIST=git-reports-history/v${NEXTNN}
    else
        # For branches, history is BRANCHNN
        set GIT_REPORT_HIST=git-reports-history/v${BRANCHNN}
    endif
endif
# check history dir exists
if ( ! -d $GIT_REPORT_HIST ) then
    echo "Error: [mode=$mode] directory does not exist [$PWD/$GIT_REPORT_HIST] on $HOST [${0}: `date`]"
    exit 1
endif
# check the 'latest' link points to right place
rm git-reports-latest
ln -s $GIT_REPORT_HIST git-reports-latest
if ( -L git-reports-latest != "${GIT_REPORT_HIST}" ) then
    echo "Error: [mode=$mode] could not make symlink [git-reports-latest -> $GIT_REPORTS_ROOT/$GIT_REPORT_HIST] on $HOST [${0}: `date`]"
    exit 1
endif
echo "Using history dir $PWD/$GIT_REPORT_HIST/ and symlink git-reports-latest on $HOST [${0}: `date`]"

cd $WEEKLYBLD

if ( "$mode" == "review") then
    set cmd="git-reports $prevTag $thisTag $TODAY $REVIEWDAY v${NEXTNN} ${BUILDHOME}/kent $GIT_REPORTS_ROOT/${GIT_REPORT_HIST} review"
else    
    if ( "$mode" == "review2") then
        set cmd="git-reports $prevTag $thisTag $REVIEWDAY $REVIEW2DAY v${NEXTNN} ${BUILDHOME}/kent $GIT_REPORTS_ROOT/${GIT_REPORT_HIST} review2"
    else    
        set cmd="git-reports $prevTag $thisTag $REVIEW2DAY $TODAY v${BRANCHNN} ${BUILDHOME}/kent $GIT_REPORTS_ROOT/${GIT_REPORT_HIST} branch"
    endif    
endif    

echo "$cmd"
$cmd
if ( $status ) then
    echo "Error: [mode=$mode] $cmd failed on $HOST [${0}: `date`]"
    exit 1
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
    if ( "$mode" == "review2") then
        echo "<li><a href="review2/index.html">Design/Review2 - Day 9 - v$NEXTNN</a> ($REVIEWDAY to $REVIEW2DAY)" >> index.html
        echo "<li><a href="review/index.html">Design/Review - Day 2 - v$NEXTNN</a> ($TODAY to $REVIEWDAY)" >> index.html
        echo "<li><a href="/git-reports-history/">Previous versions</a> " >> index.html
    else
        echo "<li><a href="branch/index.html">Branch - Day 16 - v$BRANCHNN</a> ($REVIEW2DAY to $TODAY)" >> index.html
        echo "<li><a href="review2/index.html">Design/Review2 Branch - Day 9 - v$BRANCHNN</a> ($REVIEWDAY to $REVIEW2DAY)" >> index.html
        echo "<li><a href="review/index.html">Design/Review - Day 2 - v$BRANCHNN</a> ($LASTWEEK to $REVIEWDAY)" >> index.html
        echo "<li><a href="/git-reports-history/">Previous versions</a>" >> index.html
    endif    
endif    
echo "</body></html>" >> index.html
cd $WEEKLYBLD

echo "success Git Reports v${BRANCHNN} $mode [${0}: START=${ScriptStart} END=`date`]" > GitReports.ok

exit 0


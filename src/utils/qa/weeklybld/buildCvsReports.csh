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

if ( -e CvsReports.ok ) then
    rm CvsReports.ok
endif    

echo
echo "now building CVS reports on $HOST. [${0}: `date`]"

@ LASTNN=$BRANCHNN - 1
set fromTag=v${LASTNN}_branch
set toTag=v${BRANCHNN}_branch
set branchTag="branch"
set reviewTag="review"

if ( "$BRANCHNN" == "" ) then
 echo "BRANCHNN undefined."
 exit 1
endif
if ( "$LASTNN" == "" ) then
 echo "LASTNN undefined."
 exit 1
endif

echo "BRANCHNN=$BRANCHNN"
echo "LASTNN=$LASTNN"
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

# Keep a history of cvs-reports by creating a new directory, and setting a symlink (cvs-reports-latest) to it. 
# The webserver has symlinks to the latest one and the history:
#   /usr/local/apache/htdocs/cvs-reports -> /cluster/hive/groups/qa/cvs-reports-latest
#   /usr/local/apache/htdocs/cvs-reports-history -> /hive/groups/qa/cvs-reports-history

# The script which builds the repoerts (cvs-reports-delta) also has a setting which points to this: 
#     'CVS_REPORTS_BASE=/hive/groups/qa/cvs-reports-latest'

set CVS_REPORTS_ROOT=/hive/groups/qa/
cd $CVS_REPORTS_ROOT

if ( "$mode" == "review") then
    # Preview is for BRANCHNN+1
    # Make the history directories and change the 'latest' link
    @ NEXTNN=$BRANCHNN + 1
    set CVS_REPORT_HIST=cvs-reports-history/v${NEXTNN}
    set CVS_REPORT_OLD_HIST=cvs-reports-history/v${BRANCHNN}
    if ( ! -d $CVS_REPORT_HIST ) then
        mkdir $CVS_REPORT_HIST
        if ( $status ) then
	    echo "Error: [mode=$mode] could not make directory [$PWD/$CVS_REPORT_HIST] on $HOST [${0}: `date`]"
	    exit 1
        endif
        mkdir $CVS_REPORT_HIST/branch
        mkdir $CVS_REPORT_HIST/review
        mkdir $CVS_REPORT_HIST/kent
        echo "Moving previous cvs for update by cvs-reports-d [from $CVS_REPORT_OLD_HIST/kent/* -> $CVS_REPORT_HIST/kent/] on $HOST [${0}: `date`]"
	mv $CVS_REPORT_OLD_HIST/kent/* $CVS_REPORT_HIST/kent/
	echo "Created  dirs $CVS_REPORT_HIST/{branch,review,kent} on $HOST [${0}: `date`]"
    endif
else
    # For branches, history is BRANCHNN
    set CVS_REPORT_HIST=cvs-reports-history/v${BRANCHNN}
endif
# check history dir exists
if ( ! -d $CVS_REPORT_HIST ) then
    echo "Error: [mode=$mode] directory does not exist [$PWD/$CVS_REPORT_HIST] on $HOST [${0}: `date`]"
    exit 1
endif
# check the 'lastest' link points to right place
rm cvs-reports-latest
ln -s $CVS_REPORT_HIST cvs-reports-latest
if ( -L cvs-reports-latest != "${CVS_REPORT_HIST}" ) then
    echo "Error: [mode=$mode] could not make symlink [cvs-reports-latest -> $CVS_REPORTS_ROOT/$CVS_REPORT_HIST] on $HOST [${0}: `date`]"
    exit 1
endif
echo "Using history dir $PWD/$CVS_REPORT_HIST/ and symlink cvs-reports-latest on $HOST [${0}: `date`]"

# it will shove itself off into the background anyway!
cd $WEEKLYBLD

# You can't check the status code of a command after an if statement, it must be directly after the command
if ( "$mode" == "review") then
    ./cvs-reports-delta $branchTag $reviewTag $TODAY $REVIEWDAY review v${BRANCHNN}
    if ( $status ) then
        echo "Error: [mode=$mode] cvs-reports-delta $branchTag $reviewTag $TODAY $REVIEWDAY review v${BRANCHNN} failed on $HOST [${0}: `date`]"
        exit 1
    endif
else    
    ./cvs-reports-delta $reviewTag $branchTag $REVIEWDAY $TODAY branch v${BRANCHNN}
    if ( $status ) then
        echo "Error: [mode=$mode] cvs-reports-delta $reviewTag $branchTag $REVIEWDAY $TODAY branch v${BRANCHNN} failed on $HOST [${0}: `date`]"
        exit 1
    endif
endif    

echo "cvs-reports-delta done on $HOST [${0}: `date`]"

cd $WEEKLYBLD

# fix main report page /cvs-reports/index.html to have dates
cd /usr/local/apache/htdocs/cvs-reports/
echo "<html><head><title>cvs-reports</title></head><body>" > index.html
echo "<h1>CVS changes: kent</h1>" >> index.html
echo "<ul>" >> index.html
if ( "$mode" == "review") then
    echo "<li><a href="review/index.html">Design/Review - Day 2</a> ($TODAY to $REVIEWDAY)" >> index.html
    echo "<li><a href="branch/index.html">Biweekly Branch - Day 9</a> ($LASTREVIEWDAY to $TODAY)" >> index.html
else
    echo "<li><a href="branch/index.html">Biweekly Branch - Day 9</a> ($REVIEWDAY to $TODAY)" >> index.html
    echo "<li><a href="review/index.html">Design/Review - Day 2</a> ($LASTWEEK to $REVIEWDAY)" >> index.html
endif    
echo "</ul><a href="/cvs-reports-history/">Reports history</a>." >> index.html
echo "</body></html>" >> index.html
cd $WEEKLYBLD

echo "success CVS Reports v${BRANCHNN} $mode [${0}: START=${ScriptStart} END=`date`]" > CvsReports.ok

exit 0


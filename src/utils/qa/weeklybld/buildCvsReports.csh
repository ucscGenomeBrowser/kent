#!/bin/tcsh
if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev."
 exit 1
endif

set mode=$1
if ( "$mode" != "branch" && "$mode" != "review") then
  echo "must specify the mode (without quotes) on commandline: either 'review' (day1) or 'branch' (day8) "
  exit 1
endif

# ASSUMPTION: that the requisite branch-tag or review-tag script has already been run.

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

echo
echo "now building CVS reports."

@ LASTNN=$BRANCHNN - 1
set fromTag=v${LASTNN}_branch
set toTag=v${BRANCHNN}_branch
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

if ( "$2" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

# it will shove itself off into the background anyway!
cd /projects/compbio/bin

if ( "$mode" == "review") then
    ./cvs-reports-delta $toTag $reviewTag $TODAY $REVIEWDAY review
else    
    ./cvs-reports-delta $reviewTag $toTag $REVIEWDAY $TODAY branch
endif    
cd $WEEKLYBLD

# fix main report page /cvs-reports/index.html to have dates
if ( "$mode" == "review") then
    set cmd="s/<\!-- comment1Start -->.*<\!-- comment1End -->/<\!-- comment1Start -->($TODAY to $REVIEWDAY)<\!-- comment1End -->/"
else    
    set cmd="s/<\!-- comment2Start -->.*<\!-- comment2End -->/<\!-- comment2Start -->($REVIEWDAY to $TODAY)<\!-- comment2End -->/"
endif    
sed -e "$cmd" /usr/local/apache/htdocs/cvs-reports/index.html > index.html
cp index.html /usr/local/apache/htdocs/cvs-reports/index.html 
#rm index.html

if ( $status ) then
 echo "cvs-reports-delta failed on $HOST"
 exit 1
endif
echo "cvs-reports-delta done on $HOST"

exit 0


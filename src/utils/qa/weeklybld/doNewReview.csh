#!/bin/tcsh
#
# WARNING: this does not like to be run with final & to detach from terminal.
#  Heather says perhaps running with "nohup" in front of the command might
#  make it work better.
#
if ( "$HOST" != "hgwbeta" ) then
 echo "Error: this script must be run from hgwbeta."
 exit 1
endif

cd $WEEKLYBLD

echo "REVIEWDAY=$REVIEWDAY   (review day, day2)"
echo "LASTREVIEWDAY=$LASTREVIEWDAY   (previous review day, day2)"
echo "BRANCHNN=$BRANCHNN"
echo "TODAY=$TODAY   (last build day)"
echo "LASTWEEK=$LASTWEEK   (previous build day)"


if ( "$TODAY" == "" ) then
 echo "TODAY undefined."
 exit 1
endif
if ( "$BRANCHNN" == "" ) then
 echo "BRANCHNN undefined."
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

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

echo
echo "Now beginning to build new branch $BRANCHNN [${0}: `date`]"

echo

#echo debug: disabled tagging
./tagReview.csh real
if ( $status ) then
 echo "tagReview.csh failed on $HOST [${0}: `date`]"
 exit 1
endif
echo "tagReview.csh done on $HOST [${0}: `date`]"
echo "tag review moved to HEAD."

#echo debug: disabled buildCvsReports
ssh -n hgwdev "$WEEKLYBLD/buildCvsReports.csh review real"
if ( $status ) then
 echo "buildCvsReports.csh  failed on hgwdev [${0}: `date`]"
 exit 1
endif
echo "buildCvsReports.csh done on hgwdev, sending email... [${0}: `date`]"
echo "Ready for pairings (day 2, CVS reports completed for v$BRANCHNN preview)." | mail -s "Ready for pairings (day 2, CVS reports completed for v$BRANCHNN preview)." $USER donnak kate kuhn


#---------------------

exit 0


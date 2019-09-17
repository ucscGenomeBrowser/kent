#!/bin/tcsh
#
# WARNING: this does not like to be run with final & to detach from terminal.
#  Heather says perhaps running with "nohup" in front of the command might
#  make it work better.
#

cd $WEEKLYBLD

echo "REVIEW2DAY=$REVIEW2DAY   (review2 day, day9)"
echo "LASTREVIEW2DAY=$LASTREVIEW2DAY   (previous review2 day, day9)"
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
if ( "$REVIEW2DAY" == "" ) then
 echo "REVIEW2DAY undefined."
 exit 1
endif
if ( "$LASTREVIEW2DAY" == "" ) then
 echo "LASTREVIEW2DAY undefined."
 exit 1
endif

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

@ NEXTNN = ( $BRANCHNN + 1 )
echo
echo "Now beginning to build preview2 of branch $NEXTNN [${0}: `date`]"

echo

#echo debug: disabled tagging
./tagPreview2.csh real
if ( $status ) then
 echo "tagPreview2.csh failed on $HOST [${0}: `date`]"
 exit 1
endif
echo "tagPreview2.csh done on $HOST [${0}: `date`]"
echo "tag preview2 moved to HEAD."

#echo debug: disabled buildGitReports
ssh -n hgwdev "$WEEKLYBLD/buildGitReports.csh review2 real"
if ( $status ) then
 echo "buildGitReports.csh  failed on hgwdev [${0}: `date`]"
 exit 1
endif


echo "buildGitReports.csh done on hgwdev, sending email... [${0}: `date`]"

#echo debug: disabled sending email
echo "Ready for pairings, day 9, Git reports completed for v${NEXTNN} preview2 http://genecats.gi.ucsc.edu/git-reports/ (history at http://genecats.gi.ucsc.edu/git-reports-history/)." | mail -s "Ready for pairings (day 9, v${NEXTNN} preview2)." ${BUILDMEISTEREMAIL} kuhn@soe.ucsc.edu brianlee@soe.ucsc.edu azweig@ucsc.edu kate@soe.ucsc.edu braney@soe.ucsc.edu chmalee@ucsc.edu jnavarr5@ucsc.edu


#---------------------

exit 0


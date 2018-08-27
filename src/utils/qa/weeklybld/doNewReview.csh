#!/bin/tcsh
#
# WARNING: this does not like to be run with final & to detach from terminal.
#  Heather says perhaps running with "nohup" in front of the command might
#  make it work better.
#

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

@ NEXTNN = ( $BRANCHNN + 1 )
echo
echo "Now beginning to build preview of branch $NEXTNN [${0}: `date`]"

echo

#echo debug: disabled tagging
./tagPreview.csh real
if ( $status ) then
 echo "tagPreview.csh failed on $HOST [${0}: `date`]"
 exit 1
endif
echo "tagPreview.csh done on $HOST [${0}: `date`]"
echo "tag preview moved to HEAD."

#echo debug: disabled buildGitReports
ssh -n hgwdev "$WEEKLYBLD/buildGitReports.csh review real"
if ( $status ) then
 echo "buildGitReports.csh  failed on hgwdev [${0}: `date`]"
 exit 1
endif


echo "buildGitReports.csh done on hgwdev, sending email... [${0}: `date`]"

#echo debug: disabled sending email
echo "Ready for pairings, day 2, Git reports completed for v${NEXTNN} preview http://genecats.gi.ucsc.edu/git-reports/ (history at http://genecats.gi.ucsc.edu/git-reports-history/)." | mail -s "Ready for pairings (day 2, v${NEXTNN} preview)." ${BUILDMEISTEREMAIL} kuhn@soe.ucsc.edu brianlee@soe.ucsc.edu azweig@ucsc.edu kate@soe.ucsc.edu braney@soe.ucsc.edu chmalee@ucsc.edu jnavarr5@ucsc.edu


#---------------------

exit 0


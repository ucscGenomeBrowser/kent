#!/bin/tcsh
if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev."
 exit 1
endif

cd $WEEKLYBLD

if ( "$TODAY" == "" ) then
 echo "TODAY undefined."
 exit 1
endif
if ( "$LASTWEEK" == "" ) then
 echo "LASTWEEK undefined."
 exit 1
endif

echo
echo "now building CVS reports."

@ LASTNN=$BRANCHNN - 1
set fromTag=v${LASTNN}_branch
set toTag=v${BRANCHNN}_branch

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

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

# it will shove itself off into the background anyway!
cd /projects/compbio/bin
./cvs-reports-delta $fromTag $toTag $LASTWEEK $TODAY
if ( $status ) then
 echo "cvs-reports-delta failed on $HOST"
 exit 1
endif
echo "cvs-reports-delta done on $HOST"

exit 0


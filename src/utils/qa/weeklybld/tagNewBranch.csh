#!/bin/tcsh
if ( "$HOST" != "kkstore" ) then
 echo "Error: this script must be run from kkstore."
 exit 1
endif

cd $WEEKLYBLD

echo "BRANCHNN=$BRANCHNN"
echo "TODAY=$TODAY"
echo "LASTWEEK=$LASTWEEK"

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

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

echo "Tagging new branch $BRANCHNN"

# rtag the new branch
set temp = "v$BRANCHNN""_branch"
cvs -d hgwdev:$CVSROOT rtag -b "$temp" kent >& /dev/null
if ( $status ) then
 echo "cvs rtag failed for $MYCVS/$TODAY/kent with new version# $BRANCHNN on $HOST"
 exit 1
endif
echo "new branch and tag v$BRANCHNN created."

echo
echo "moving tag beta..."
cvs -d hgwdev:$CVSROOT rtag -dF beta kent >& /dev/null
cvs -d hgwdev:$CVSROOT rtag -rv${BRANCHNN}_branch beta kent >& /dev/null
echo "beta regular tag moved to the new branch v$BRANCHNN."

exit 0


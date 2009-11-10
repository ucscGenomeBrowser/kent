#!/bin/tcsh
if ( "$HOST" != "hgwbeta" ) then
 echo "Error: this script must be run from hgwbeta."
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
	echo "Not real.   To make real changes, put real as cmdline parm. [${0}: `date`]"
	echo
	exit 0
endif 

echo "Tagging beta tag to branch $BRANCHNN [${0}: `date`]"

# new way (faster, but does it work? attic files ok?):
cvs -d hgwdev:$CVSROOT rtag -Fa -rv${BRANCHNN}_branch beta kent >& /dev/null
# old way (works but slower)
#cvs -d hgwdev:$CVSROOT rtag -da beta kent >& /dev/null
#cvs -d hgwdev:$CVSROOT rtag -rv${BRANCHNN}_branch beta kent >& /dev/null
if ( $status ) then
 echo "cvs rtag failed for beta tag with new version# $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif

echo "1" > pushedToRR.flag

echo "beta regular tag moved to the new branch v$BRANCHNN. [${0}: `date`]"

exit 0


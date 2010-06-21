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

git tag -d beta   # delete local tag
git fetch
git push origin :beta   # delete old beta tag on shared repo
git fetch
git push origin origin/v${BRANCHNN}_branch:refs/tags/beta   # create new tag at current branch tip
if ( $status ) then
 echo "git shared-repo tag failed for beta tag with branch $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif
git fetch

echo "1" > pushedToRR.flag

echo "beta tag moved to the new branch v$BRANCHNN. [${0}: `date`]"

exit 0


#!/bin/tcsh
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

echo "Tagging new branch $BRANCHNN [${0}: `date`]"

# tag the branch base
#  which marks original branch point for git reports
#  this should not fail typically so -f to force not needed
git push origin origin:refs/tags/v${BRANCHNN}_base
if ( $status ) then
 echo "git shared-repo tag failed for tag v${BRANCHNN}_base with new version# $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif
echo "new tag branch created. [${0}: `date`]"
git fetch   # required

# create the branch in the shared repo
git push origin v${BRANCHNN}_base:refs/heads/v${BRANCHNN}_branch
if ( $status ) then
 echo "git shared-repo branch creation failed for branch-tag v${BRANCHNN}_branch with new version# $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif
echo "new branch and tag v$BRANCHNN created. [${0}: `date`]"

if (-e pushedToRR.flag ) then
    rm pushedToRR.flag
endif

exit 0


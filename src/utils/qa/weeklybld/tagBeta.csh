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

echo "Re-setting beta branch to branch $BRANCHNN [${0}: `date`]"

# see if we are on master already
set x = `git branch|grep '^. master$'`  # asterisk inside grep expression causes trouble
if ("$x" != "* master") then
    git checkout master  # get back to master branch
    if ( $status ) then
     echo "git checkout master: failed for beta tag with branch $BRANCHNN on $HOST [${0}: `date`]"
     exit 1
    endif
endif
git fetch
if ( $status ) then
 echo "git fetch: failed for beta tag with branch $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif
# see if old beta branch exists
set x = `git branch|grep '^  beta$'`
if ("$x" == "beta") then
    git branch -D beta   # delete any old local beta branch
    if ( $status ) then
     echo "git branch -D beta: failed for beta tag with branch $BRANCHNN on $HOST [${0}: `date`]"
     exit 1
    endif
endif
git checkout -b beta origin/v${BRANCHNN}_branch  # the new one where we want to be
if ( $status ) then
 echo "git checkout -b beta origin/v${BRANCHNN}_branch: failed for beta tag with branch $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif
git merge -s ours origin/beta  # special merge to ignore any commits on the old branch
if ( $status ) then
 echo "git merge -s ours origin/beta: failed for beta tag with branch $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif
git push origin beta    # can push beta to central repo now that origin/beta will be an ancestor
if ( $status ) then
 echo "git push origin beta: failed for beta tag with branch $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif
git checkout master  # get back to master branch
if ( $status ) then
 echo "git checkout master: failed for beta tag with branch $BRANCHNN on $HOST [${0}: `date`]"
 exit 1
endif
git branch -D beta   # delete any old local beta branch

echo "1" > pushedToRR.flag

echo "beta branch reset to the new branch v$BRANCHNN. [${0}: `date`]"

exit 0


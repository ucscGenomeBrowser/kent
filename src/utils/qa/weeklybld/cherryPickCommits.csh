#!/bin/tcsh

cd $WEEKLYBLD

set table=CherryPickCommits.conf
if ( ! -e $table ) then
    echo "error: $table not found."
    exit 1
endif

echo
echo "BRANCHNN=$BRANCHNN"

#make sure origin/* is up to date
echo "git fetch to be sure origin/* is up-to-date"
git fetch
if ($status) then
    echo "unexpected error running git fetch."
    exit 1
endif

#see if branch exists
set x=`git branch | grep "^. v${BRANCHNN}_branch"`
if ("$x" == "") then
    # branch does not exist, create it
    echo "v${BRANCHNN}_branch does not exist, creating as a tracking branch."
    git checkout -tb v${BRANCHNN}_branch origin/v${BRANCHNN}_branch
    if ($status) then
	echo "error running: git checkout -tb v${BRANCHNN}_branch origin/v${BRANCHNN}_branch"
	exit 1
    endif	
else
    if ("$x" != "* v${BRANCHNN}_branch") then
	# switch to branch
	echo "v${BRANCHNN}_branch exists, checking it out."
	git checkout v${BRANCHNN}_branch 
	if ($status) then
	    echo "error running:  git checkout v${BRANCHNN}_branch"
	    exit 1
	endif
    else
	echo "v${BRANCHNN}_branch exists, and is already checked out."
    endif
endif

#check working directory is clean
echo "Checking: is working directory clean?"
git diff --stat --exit-code
if ($status) then
    echo "error: working directory not clean, changed tracked files in working directory"
    exit 1
endif
git diff --stat --cached --exit-code
if ($status) then
    echo "error: working directory not clean, files have been staged but not committed"
    exit 1
endif
echo "Working directory clean."

set dir="${BUILDDIR}/v${BRANCHNN}_branch/kent/src"

set err=0
set msg = ""
echo
echo "Verifying commits exist and have the right changes in them:"
echo ""
echo "NOTE: DO NOT INCLUDE MERGE COMMITS IN YOUR CHERRY PICKS"
echo ""
set list = (`cat {$table}`)
if ( $#list == 0 ) then
    echo "Error: $table is empty. Put commits to be cherry-picked onto v${BRANCHNN}_branch into it, one per line."
    exit 1
endif
set errpull64 = "0"
while ( $#list > 0 )
    set c = $list[1]
    shift list
    git show --stat $c
    if ($status) then
	echo "failed running: git show --stat $c"
	echo "You might get this error you need to fetch or pull on master brancha: fatal: bad object <commitId>"
	exit 1
    endif
    if ( "$1" == "real" ) then

	# cherry pick one commit
	echo "cherry-picking one commit $c onto v${BRANCHNN}_branch"
	git cherry-pick -x $c
	if ($status) then
	    echo "! ! ! failed running: git cherry-pick -x $c"
	    echo "This may be because of conflicts which you will need to resolve and commit."
	    echo "You might get this error if the changes in the commit specified have already been cherry-picked or patched into the branch."
	    exit 1
	endif

	# push to world
	echo "pushing v${BRANCHNN}_branch to origin"
	git push origin v${BRANCHNN}_branch
	if ($status) then
	    echo "! ! ! failed running: git push (from v${BRANCHNN}_branch)"
	    echo "This is an unexpected error you may need to investigate who has been messing with v${BRANCHNN}_branch."
	    exit 1
	endif	

	if (-d $BUILDDIR/v${BRANCHNN}_branch) then
    	    # log the cherry-pick
    	    echo "logging the cherry-pick to $BUILDDIR/v${BRANCHNN}_branch/cherryPicks.log"
    	    date +%Y-%m-%d >> $BUILDDIR/v${BRANCHNN}_branch/cherryPicks.log
    	    echo "$c"      >> $BUILDDIR/v${BRANCHNN}_branch/cherryPicks.log
	endif

	# email the cherry-pick
	echo "emailing the cherry-pick"
	set mailMsg = "Commit $c has been cherry-picked onto v${BRANCHNN} branch"
	git show --stat $c | mail -s "$mailMsg" ${BUILDMEISTEREMAIL} galt@soe.ucsc.edu browser-qa@soe.ucsc.edu

	if (-d $BUILDDIR/v${BRANCHNN}_branch) then
	    # do git pull in 64-bit build repo	
    	    echo "doing git pull in 64-bit build repo"
    	    pushd "${BUILDDIR}/v${BRANCHNN}_branch/kent/src"
	    git pull
	    if ($status) then
		echo "failed running: git pull (in 64-bit build repo ${BUILDDIR}/v${BRANCHNN}_branch/kent/src)"
    		set errpull64 = "1"
    	    endif
    	    popd 
	endif

	if (-e pushedToRR.flag ) then
	    # TODO when all is working smoothly: move the beta branch to this week's branch.
	    # maybe we can run tagBeta.csh?
	    echo "TODO: You will need to run tagBeta.csh to update the beta branch."
	    # TODO maybe run the sub-version tagger automatically too someday.
	    echo "TODO: You will need to run whatever process to update the branch subversion."
	endif

    endif
    echo ""
end

echo "All commits specified found."

if ( "$errpull64" == "1" ) then
    echo "error updating 64-bit sandbox."
    exit 1
endif

# return to master branch
echo "Returning to master branch"
git checkout master
if ($status) then
    echo "error running:  git checkout master"
    exit 1
endif

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

exit 0


#!/bin/tcsh
#following line using source .cshrc doesnt really work?
cd $WEEKLYBLD

if ( "$HOST" != "hgwbeta" ) then
 echo "error: you must run this script on beta!"
 exit 1
endif

set table=MoveBranchTagFiles.conf
if ( ! -e $table ) then
    echo "$table not found."
    exit 1
endif

echo
echo "BRANCHNN=$BRANCHNN"

set dir="${BUILDDIR}/v${BRANCHNN}_branch/kent/src"

set err=0
set msg = ""
echo
echo "Verifying files exist in $dir"
set list = (`cat {$table}`)
set files = ()
set revs = ()
set prevs = ()
while ( $#list > 0 )
    set f = $list[1]
    shift list
    set r = "HEAD"
    set p = "none"
    if ( $#list > 0 ) then
    	if ( $list[1] =~ [0123456789.]* ) then
    	    set r = $list[1]
    	    shift list
	endif
    endif
    # check for the previous revision
    if ( -e $dir/$f ) then
    	set filter = "\/$f:t\/"
	#echo "filter=$filter" 
    	grep $filter  $dir/$f:h/CVS/Entries | awk -F '/' '{print $3}' > $WEEKLYBLD/tempver
	set p = `cat $WEEKLYBLD/tempver`
    	rm -f $WEEKLYBLD/tempver
    else	    
	echo "$f not found on $dir"
	set err=1
    endif
    echo "$f $p --> $r"
    set files = ($files $f)
    set revs = ($revs $r)
    set prevs = ($prevs $p)
end
if ( "$err" == "1" ) then
 echo "some files not found."
 # this may be a problem for brand-new files???-rare
 if ( "$2" != "override" ) then
	echo
	echo "No override.   To override, specify after real on cmdline parm."
	echo
	exit 1
 else
    echo "All files specified found."
 endif 
endif

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 


cd $dir
set err=0
echo
echo "Moving tag to target rev and updating ..."
set i = 1
while ( $i <= $#files )
    echo
    set f = $files[$i]
    set r = $revs[$i]
    set p = $prevs[$i]

# I MIGHT NOT NEED THIS
#    if ( "$r" == "$p" ) then
#	# if moving to where it already is, 
#	# we have to remove it first to establish a new branch,
#	# otherwise it does nothing and you are left with the old branch
#	# including any failed patches, which is not good,
#	# so we must remove it, so that when it gets added back,
#	# it will get a new branch revision number.
#	set cmd = "cvs rtag -dB v${BRANCHNN}_branch kent/src/$f"
#	echo $cmd
#	$cmd
#	if ( $status ) then 
#	    echo "error moving cvs branch tag for $f"
#	    set err=1
#	    break
#	endif
#    endif

    # move the tag in cvs for this week's branch.
    set cmd = "cvs rtag -r${r} -F -B -b v${BRANCHNN}_branch kent/src/$f"
    echo $cmd
    $cmd
    if ( $status ) then 
	echo "error moving cvs branch tag for $f"
	set err=1
	break
    endif
    # move the beta tag in cvs to track the change to this week's branch.
    set cmd = "cvs rtag -rv${BRANCHNN}_branch -F beta kent/src/$f"
    echo $cmd
    $cmd
    if ( $status ) then 
	echo "error moving cvs beta tag for $f"
	set err=1
	break
    endif
    # update the file from cvs branch in branch sandbox.
    if ( -d $dir/$f:h ) then
	cd $dir/$f:h    # just dir and update one file
    	set cmd = "cvs up -dP $f:t"
	# update 32-bit sandbox too
	set cmd32 = "cd /scratch/releaseBuild/v${BRANCHNN}_branch/kent/src/$f:h;cvs up $f:t"
    else
	cd $dir/$f:h:h   # just go to parent and update because dir does not exist yet
	set cmd = "cvs up -dP $f:h:t"
	# update 32-bit sandbox too
	set cmd32 = "cd /scratch/releaseBuild/v${BRANCHNN}_branch/kent/src/$f:h:h;cvs up $f:h:t"
    endif
    pwd
    echo $cmd
    $cmd # restore: >& /dev/null
    if ( $status ) then 
	echo "error in cvs update of $f"
	set err=1
	break
    endif
    # update 32bit sandbox on $BOX32 too
    echo "$cmd32"
    #old way: ssh $BOX32 "$cmd32"
    ssh $BOX32 "/bin/tcsh -c '"$cmd32"'"
    set msg = "$msg $f $p --> $r\n"
    @ i++
end
if ( "$err" == "1" ) then
    echo "error updating."
    exit 1
endif

set mailMsg = "The v${BRANCHNN} branch-tag has been re-moved to the following:\n$msg"
set subject = '"'"Branch tag move complete."'"'
echo "$mailMsg" | mail -s "$subject" $USER galt browser-qa

date +%Y-%m-%d   >> $BUILDDIR/v${BRANCHNN}_branch/branchMoves.log
echo "$msg"    >> $BUILDDIR/v${BRANCHNN}_branch/branchMoves.log
exit 0


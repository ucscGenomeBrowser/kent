#!/bin/tcsh
#following line using source .cshrc doesnt really work?
cd $WEEKLYBLD

if ( "$HOST" != "hgwdev" ) then
 echo "error: you must run this script on dev!"
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
while ( $#list > 0 )
    set f = $list[1]
    shift list
    set r = "HEAD"
    if ( $#list > 0 ) then
    	if ( $list[1] =~ [0123456789.]* ) then
    	    set r = $list[1]
    	    shift list
	endif
    endif
    if ( ! -e $dir/$f ) then
	echo "$f not found on $dir"
	set err=1
    endif 
    echo "$f $r"
    set files = ($files $f)
    set revs = ($revs $r)
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
    cd $dir/$f:h
    pwd
    # move the tag in cvs for this week's branch.
    set cmd = "cvs tag -r${r} -F -B -b v${BRANCHNN}_branch $f:t"
    echo $cmd
    $cmd
    if ( $status ) then 
	echo "error moving cvs branch tag for $dir/$f"
	set err=1
	break
    endif
    echo $f >> $WEEKLYBLD/mbt-tag.txt
    # move the beta tag in cvs to track the change to this week's branch.
    set cmd = "cvs tag -rv${BRANCHNN}_branch -F beta $f:t"
    echo $cmd
    $cmd
    if ( $status ) then 
	echo "error moving cvs beta tag for $dir/$f"
	set err=1
	break
    endif
    # update the file from cvs branch in branch sandbox.
    set cmd = "cvs up -rv${BRANCHNN}_branch -dP $f:t"
    echo $cmd
    $cmd
    if ( $status ) then 
	echo "error in cvs update of $dir/$f"
	set err=1
	break
    endif
    echo $f >> $WEEKLYBLD/mbt-up.txt  
    set msg = "$msg $f $r \n"
    @ i++
end
if ( "$err" == "1" ) then
 echo "some error updating."
 exit 1
endif

set msg = "The v${BRANCHNN} branch-tag has been re-moved to the following:\n $msg"
set subject = '"'"Branch tag move complete."'"'
echo "$msg" | mail -s "$subject" galt heather kuhn

exit 0


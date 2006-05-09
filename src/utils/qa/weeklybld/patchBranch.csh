#!/bin/tcsh
cd $WEEKLYBLD

echo "This script has not been updated to handle the new separate 64/32 bit sandboxes yet."
echo "Do not use this script."
exit 1

if ( "$HOST" != "hgwbeta" ) then
 echo "error: you must run this script on hgwbeta!"
 exit 1
endif

set table=~/PatchBranchFiles.conf
if ( ! -e $table ) then
    echo "$table not found."
    exit 1
endif

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
    # merge in the specified rev into this week's branch.
    set cmd = "cvs update -kk -j${r} $f:t"
    echo $cmd
    $cmd
    if ( $status ) then 
	echo "error moving cvs branch tag for $dir/$f"
	set err=1
	break
    endif
    # remove next line soon
    #echo $f >> ~/pb-merge.txt
    # commit the update to the branch sandbox.
    set commsg = '"'"merge rev $r into branch"'"'
    set cmd = "cvs commit -m "$commsg" $f:t"
    echo $cmd
    $cmd
    if ( $status ) then 
	echo "error in cvs commit of merge $r into $dir/$f"
	set err=1
	break
    endif
    # remove next line soon
    #echo $f >> ~/pb-commit.txt  
    set msg = "$msg $f $p --> $r\n"
    @ i++
end
if ( "$err" == "1" ) then
 echo "some error updating."
 exit 1
endif

set msg = "The v${BRANCHNN} branch has been merged for the following:\n $msg"
set subject = '"'"Branch merge complete."'"'
echo "$msg" | mail -s "$subject" galt heather ann
date +%Y-%m-%d   >> $BUILDDIR/v${BRANCHNN}_branch/branchPatches.log
echo "$msg"      >> $BUILDDIR/v${BRANCHNN}_branch/branchPatches.log
exit 0


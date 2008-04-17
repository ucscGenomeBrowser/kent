#!/bin/tcsh
#
# Patch files based on PatchBranchFiles.conf
#
cd $WEEKLYBLD

if ( "$HOST" != "hgwbeta" ) then
 echo "error: you must run this script on beta!"
 exit 1
endif

set table=PatchBranchFiles.conf
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
    if ( "$r" == "HEAD" ) then
	echo "Error: a single specific revision for patching in must be specified for each file, but none was specified. Correct the input."
	exit 1
    endif	    
    
    # check for the previous revision (which must exist - use moveBranchTag for new files)
    if ( -e $dir/$f ) then
    	set filter = "\/$f:t\/"
	#echo "filter=$filter" 
    	grep $filter  $dir/$f:h/CVS/Entries | awk -F '/' '{print $3}' > $WEEKLYBLD/tempver
	set p = `cat $WEEKLYBLD/tempver`
    	rm -f $WEEKLYBLD/tempver
    else	    
	echo "Error: $f not found on $dir.  \nEither input path was misspelled, or use moveBranchTag.csh for brand-new files."
	exit 1
    endif
    echo "$f $p :  patch-in $r"
    set files = ($files $f)
    set revs = ($revs $r)
    set prevs = ($prevs $p)
end

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 


cd $dir
set err=0
echo
echo "Patching-in target revs  ..."
set i = 1
while ( $i <= $#files )
    echo
    set f = $files[$i]
    set r = $revs[$i]
    set p = $prevs[$i]

    # Get previous revision (so we can limit change range to just one revision 
    #  while patching:

    echo "$r" | sed -e 's/[.][0-9]*//' > $WEEKLYBLD/tempver
    set prevInt = `cat $WEEKLYBLD/tempver`
    rm -f $WEEKLYBLD/tempver

    echo "$r" | sed -e "s/${prevInt}[.]//" | gawk '{print ($1 - 1)}' > $WEEKLYBLD/tempver
    set prev = `cat $WEEKLYBLD/tempver`
    rm -f $WEEKLYBLD/tempver

    set prev = "${prevInt}.${prev}"

    #debug
    #echo "prev = $prev and r=$r"

    # patch the file in branch sandbox.
    if (! -d $dir/$f:h ) then
	echo "unexpected error. directory $dif/$f:h does not exist."
	exit 1
    endif
    cd $dir/$f:h    # just dir and update one file
    pwd
    set cmd = "cvs up -kk -j${prev} -j${r} $f:t"
    echo $cmd
    $cmd  #debug restore: >& /dev/null
    if ( $status ) then 
	echo "error $status in cvs update patch of $f with $r in 64-bit branch sandbox."
	echo "error patching."
    	exit 1
    endif
    
    # commit the patch to the branch sandbox.
    set commsg = "'patched-in rev $r into branch'"
    #echo "debug: $commsg" 
    
    #set cmd = "cvs commit -m $commsg $f:t"
    #echo "$cmd"
    #set verbose

    set echo
    cvs commit -m "$commsg" $f:t
    set stat = $status
    unset echo
    if ( $stat ) then 
	echo "error in cvs commit of merge $r into $dir/$f"
	echo "cvs patch commmit failure for $r into $dir/$f " >> $BUILDDIR/v${BRANCHNN}_branch/branchMoves.log
	echo "YOU NEED TO RESOLVE THIS MANUALLY AND COMMIT IT IN $BUILDDIR/v${BRANCHNN}_branch/"
	echo "YOU MAY THEN NEED TO GO TO 32-BIT SANDBOX AND DO CVS UPDATE"
	exit 1
    endif
    
    # get rid of nasty sticky bit for kk left over from the patch
    set echo
    cvs up -A -rv${BRANCHNN}_branch $f:t
    set stat = $status
    unset echo
    if ( $stat ) then 
	echo "error in cvs commit of merge $r into $dir/$f"
	exit 1
    endif

    # update 32-bit sandbox on $BOX32 too
    set cmd32 = "cd /scratch/releaseBuild/v${BRANCHNN}_branch/kent/src/$f:h;cvs up $f:t"
    echo "$cmd32"
    #old way: ssh $BOX32 "$cmd32"
    ssh $BOX32 "/bin/tcsh -c '"$cmd32"'"
    
    set msg = "$msg $f $p : patched-in $r\n"
    @ i++
end

set mailMsg = "The v${BRANCHNN} branch has been patched as follows:\n$msg"
set subject = '"'"Branch patch complete."'"'
echo "$mailMsg" | mail -s "$subject" $USER hartera galt kuhn ann kayla rhead pauline 

date +%Y-%m-%d   >> $BUILDDIR/v${BRANCHNN}_branch/branchMoves.log
echo "$msg"    >> $BUILDDIR/v${BRANCHNN}_branch/branchMoves.log
exit 0


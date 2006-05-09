#!/bin/tcsh

if (( "$HOST" != "hgwbeta" ) && ( "$HOST" != "hgwdev" )) then
 echo "Error: this script must be run from hgwbeta or hgwdev."
 exit 1
endif

cd $WEEKLYBLD

echo "BRANCHNN=$BRANCHNN"
if ( "$BRANCHNN" == "" ) then
 echo "BRANCHNN undefined."
 exit 1
endif

echo
echo "now unpacking new branch $BRANCHNN on `uname -n`"

#unpack the new branch on BUILDDIR for beta
# for later: is this faster to co on kkstore or not?
cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch" 
if ( -d $dir ) then
 echo "removing old branch dir."
 rm -fr $dir 
endif
mkdir -p $dir
cd $dir
echo "Checking out branch $BRANCHNN."
cvs -d hgwdev:$CVSROOT co -r "v"$BRANCHNN"_branch"  kent >& /dev/null
set err = $status
if ( $err ) then
 echo "error running cvs co kent in $BUILDDIR/$dir : $err" 
 exit 1
endif 
echo "Done checking out branch $BRANCHNN in $BUILDDIR/$dir" 
echo "Now you should go and do build on beta!"

exit 0


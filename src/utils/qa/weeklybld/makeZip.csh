#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
 echo "error: makezip.csh must be executed from hgwbeta!"
 exit 1
endif
cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch_zip" 
if ( -e $dir ) then
 echo "removing old branch_zip dir."
 rm -fr $dir 
endif
mkdir -p $dir
cd $dir
echo "Checking out branch $BRANCHNN."
cvs -d hgwdev:$CVSROOT export -r "v"$BRANCHNN"_branch"  kent >& /dev/null
set err = $status
if ( $err ) then
 echo "error running cvs co kent: $err" 
 exit 1
endif 
set zip = "../zips/jksrc.v"$BRANCHNN".zip"
if ( -e $zip ) then
 echo "removing old zip $zip"
 rm $zip
endif
echo "zipping up $zip."
zip -rT9v0 $zip kent >& /dev/null
set err = $status
if ( $err ) then
 echo "error zipping $zip: $err" 
 exit 1
endif 
chmod 664 $zip
# remove temp directory branch_zip
cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch_zip"
if ( -e $dir ) then
    echo "cleaning up branch_zip dir."
    rm -fr $dir
endif
echo Done.
exit 0


#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwbeta" ) then
 echo "error: you must run this script on beta!"
 exit 1
endif

cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch"
cd $dir

cd kent
pwd
#
echo "Make libs."
cd src
make libs >& make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -y "error|warn" make.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "libs errs found:"
 echo "$res"
 exit 1
endif
#
echo "Make alpha."
cd hg
make alpha >& make.alpha.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -y "error|warn" make.alpha.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "alpha errs found:"
 echo "$res"
 exit 1
endif
#
exit 0


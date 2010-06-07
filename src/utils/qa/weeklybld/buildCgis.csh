#!/bin/tcsh
cd $WEEKLYBLD

if (( "$HOST" != "$BOX32" ) && ( "$HOST" != "hgwbeta" ) then
 echo "error: you must run this script on $BOX32 or hgwbeta!"
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
sed -i -e "s/-DJK_WARN//g" make.log
sed -i -e "s/-Werror//g" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log`
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
sed -i -e "s/-DJK_WARN//g" make.alpha.log
sed -i -e "s/-Werror//g" make.alpha.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.alpha.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "alpha errs found:"
 echo "$res"
 exit 1
endif
#
echo
echo "Build libs, alpha (cgi) done on $HOST"
#
exit 0


#!/bin/tcsh
#following line using source .cshrc doesnt really work?
cd $WEEKLYBLD

cd $BUILDDIR
cd zips
cd kent

echo "Make libs."
cd src
make libs >& make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -y "error|warn" make.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res"
 exit 1
endif
#
echo "Make compile."
cd hg
make compile >& make.compile.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -y "error|warn" make.compile.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res"
 exit 1
endif
#
echo "Build libs, cgi done."
exit 0


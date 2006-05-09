#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwbeta" ) then
 echo "error: you must run this script on beta!"
 exit 1
endif

# the makefile now does zoo automatically now when you call it
echo "trackDb Make strict."
cd $BUILDDIR/tip/kent/src/hg/makeDb/trackDb
make strict >& make.strict.log
set res = `/bin/egrep -i "error|warn" make.strict.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "trackDb strict errs found:"
 echo "$res"
 exit 1
endif
set res = `/bin/egrep -i "html missing" make.strict.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "trackDb strict html errs found:"
 echo "$res"
 # this is a non-critical error
 exit 0  
endif

echo "trackDb Make strict done on Beta from Tip"
exit 0


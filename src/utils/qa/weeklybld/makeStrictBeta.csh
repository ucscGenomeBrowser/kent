#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwbeta" ) then
echo "error: you must run this script on hgwbeta! [${0}: `date`]"
exit 1
endif

cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch"
cd $dir

cd kent
pwd
# the makefile now does zoo automatically now when you call it
echo "trackDb Make strict. [${0}: `date`]"
cd $BUILDDIR/$dir/kent/src/hg/makeDb/trackDb
make strict >& make.strict.log
set res = `/bin/egrep -i "error|warn" make.strict.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "trackDb strict errs found:"
 echo "$res"
 exit 1
endif

/bin/egrep -i "html missing" make.strict.log > warning.txt
set wc = `cat warning.txt | wc -w`
if ( "$wc" != "0" ) then
 echo "trackDb strict html non-fatal errs found:"
 cat warning.txt
 cat warning.txt | mail -s "v$BRANCHNN missing html error found by trackDb make strict" $USER browser-qa
endif
rm warning.txt

echo "trackDb Make strict done on Beta [${0}: `date`]"
exit 0

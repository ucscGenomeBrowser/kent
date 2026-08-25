#!/bin/tcsh

cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch"
cd $dir

cd kent
pwd
# the makefile now does zoo automatically now when you call it
echo "trackDb Make strict. [${0}: `date`]"
cd $BUILDDIR/$dir/kent/src/hg/makeDb/trackDb
# The databases are independent of each other, so run them in parallel.  -O keeps
# each database's output together in the log.  Set TRACKDB_MAKE_JOBS to change the
# job count; going much above 8 needs ssh connection sharing first, or hgwbeta's
# sshd starts refusing connections (MaxStartups).
set trackDbJobs = 8
if ( $?TRACKDB_MAKE_JOBS ) set trackDbJobs = $TRACKDB_MAKE_JOBS
echo "trackDb make with -j $trackDbJobs"
make -O -j $trackDbJobs beta >& make.strict.log
/bin/egrep -i "html missing" make.strict.log > warning.txt
/bin/egrep -iv "html missing" make.strict.log > make.strict.log2
mv make.strict.log2 make.strict.log
set res = `/bin/egrep -i "error|warn" make.strict.log | grep -v ignored`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
   echo "trackDb strict errs found:"
   echo "$res"
   tail make.strict.log
   exit 1
endif

set wc = `cat warning.txt | wc -w`
if ( "$wc" != "0" ) then
    echo "trackDb strict html non-fatal errs found:"
    cat warning.txt
    cat warning.txt | mail -s "v$BRANCHNN missing html error found by trackDb make strict" ${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu
endif
rm warning.txt

echo "trackDb Make strict done on Beta [${0}: `date`]"
exit 0

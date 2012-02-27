#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
 echo "error: doHgNearTestRobot.csh must be executed from hgwbeta!"
 exit 1
endif

cd $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/near/hgNearTest
make 
cd $WEEKLYBLD
$HOME/bin/$MACHTYPE/hgNearTest hgwbeta.cse.ucsc.edu/cgi-bin/hgNear ./logs/hgNear-v${BRANCHNN}.log

# creates branch.hgNear.log - look for unusual errors

#-- to check for errors: 
set res = `cat ./logs/hgNear-v${BRANCHNN}.log | egrep "Total"`
set wc = `echo "$res" | wc -w` 
if ( "$wc" == "0" ) then
 echo "error occurred:"
 echo "$HOME/bin/$MACHTYPE/hgNearTest failed to log any results to ./logs/hgNear-v${BRANCHNN}.log"
 mail -s "hgNearTestRobot on $HOST failed to log results to ./logs/hgNear-v${BRANCHNN}.log" $USER galt kent browser-qa
 exit 1
endif

set wc = `echo "$res" | egrep -v "0 soft errors,  0 hard errors" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res"
 echo "$res" | mail -s "Errors in hgNearTestRobot on $HOST" $USER galt kent browser-qa
 exit 1
endif
#
echo Done.
echo "$res" | mail -s "v${BRANCHNN} hgNearTest robot done successfully on $HOST." $USER galt kent browser-qa
exit 0


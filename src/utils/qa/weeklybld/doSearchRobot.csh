#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
 echo "error: doSearchRobot.csh must be executed from hgwbeta!"
 exit 1
endif

cd $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/qa
make 
cd $WEEKLYBLD
$HOME/bin/$MACHTYPE/testSearch $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/qa/search1.txt >& ./logs/search-v${BRANCHNN}.log

# creates branch.search.log - look for unusual errors

#-- to check for errors: 
set res = `cat ./logs/search-v${BRANCHNN}.log | egrep "Error" `

set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res"
 echo "$res" | mail -s "'Errors in searchRobot on $HOST'" $USER galt browser-qa
 exit 1
endif
#
echo Done.
echo "$res" | mail -s "'v${BRANCHNN} search robot done successfully on $HOST.'" $USER galt browser-qa
exit 0


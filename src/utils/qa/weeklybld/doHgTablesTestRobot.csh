#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
 echo "error: doHgTablesTestRobot.csh must be executed from hgwbeta!"
 exit 1
endif

cd $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/hgTablesTest
make 
cd $WEEKLYBLD
$HOME/bin/$MACHTYPE/hgTablesTest hgwbeta.cse.ucsc.edu/cgi-bin/hgTables ./logs/hgTables-v${BRANCHNN}.log

# creates hgTables.log - look for unusual errors

#-- to check for errors: 
set res = `cat ./logs/hgTables-v${BRANCHNN}.log | egrep "Total" | egrep "0 soft errors,  0 hard errors"`
set res2 = `cat ./logs/hgTables-v${BRANCHNN}.log | egrep "Total"`

set wc = `echo "$res" | wc -w` 
if ( "$wc" == "0" ) then
 echo "errs found:"
 echo "$res2"
# echo "$res2" | mail -s "'Errors in hgTablesTestRobot on $HOST'" $USER hartera galt kuhn kent ann
 echo "$res2" | mail -s "'Errors in hgTablesTestRobot on $HOST'" $USER ann rhead
 exit 1
endif
#
echo Done.
#echo "$res2" | mail -s "'v${BRANCHNN} hgTablesTest robot done successfully on $HOST.'" $USER hartera galt kuhn kent ann
echo "$res2" | mail -s "'v${BRANCHNN} hgTablesTest robot done successfully on $HOST.'" $USER ann rhead
exit 0


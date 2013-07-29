#!/bin/tcsh
cd $WEEKLYBLD

cd $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/hgTablesTest
make 
cd $WEEKLYBLD

echo "$HOME/bin/$MACHTYPE/hgTablesTest -org=Human -orgs=1  hgwbeta.cse.ucsc.edu/cgi-bin/hgTables ./logs/hgTables-v${BRANCHNN}.log" > ./logs/hgTables-v${BRANCHNN}.log
$HOME/bin/$MACHTYPE/hgTablesTest -appendLog -org=Human -orgs=1  hgwbeta.cse.ucsc.edu/cgi-bin/hgTables ./logs/hgTables-v${BRANCHNN}.log
echo "" >> ./logs/hgTables-v${BRANCHNN}.log
echo "$HOME/bin/$MACHTYPE/hgTablesTest -appendLog -org=Mouse -orgs=1  hgwbeta.cse.ucsc.edu/cgi-bin/hgTables ./logs/hgTables-v${BRANCHNN}.log" >> ./logs/hgTables-v${BRANCHNN}.log
$HOME/bin/$MACHTYPE/hgTablesTest -appendLog -org=Mouse -orgs=1  hgwbeta.cse.ucsc.edu/cgi-bin/hgTables ./logs/hgTables-v${BRANCHNN}.log

# creates hgTables.log - look for unusual errors

#-- to check for errors: 
set res = `cat ./logs/hgTables-v${BRANCHNN}.log | egrep "^[ \t]*Total" | egrep -v "0 soft errors,  0 hard errors"`
set res2 = `cat ./logs/hgTables-v${BRANCHNN}.log | awk '/^[ \t]*Total/ {print} /TablesTest/ {print}'`


set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res2"
 echo "$res2" | mail -s "Errors in hgTablesTestRobot on $HOST" $USER ${BUILDMEISTER} rhead pauline luvina
 exit 1
endif
#
echo Done.
echo "$res2" | mail -s "v${BRANCHNN} hgTablesTest robot done successfully on $HOST." $USER ${BUILDMEISTER} rhead pauline luvina
exit 0


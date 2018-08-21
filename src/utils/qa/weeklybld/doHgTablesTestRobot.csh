#!/bin/tcsh
cd $WEEKLYBLD

cd $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/hgTablesTest
make 
cd $WEEKLYBLD

setenv HGDB_CONF /cluster/home/build/.hg.conf.beta
set log = v${BRANCHNN}.hgTables.log

echo "$HOME/bin/$MACHTYPE/hgTablesTest -db=hg38 hgwbeta.soe.ucsc.edu/cgi-bin/hgTables ./logs/$log" > ./logs/$log
$HOME/bin/$MACHTYPE/hgTablesTest -appendLog -db=hg38 hgwbeta.soe.ucsc.edu/cgi-bin/hgTables ./logs/$log
echo "" >> ./logs/$log
echo "$HOME/bin/$MACHTYPE/hgTablesTest -appendLog -org=Mouse -orgs=1  hgwbeta.soe.ucsc.edu/cgi-bin/hgTables ./logs/$log" >> ./logs/$log
$HOME/bin/$MACHTYPE/hgTablesTest -appendLog -org=Mouse -orgs=1  hgwbeta.soe.ucsc.edu/cgi-bin/hgTables ./logs/$log

# creates hgTables.log - look for unusual errors

#-- to check for errors: 
set res = `cat ./logs/$log | egrep "^[ \t]*Total" | egrep -v "0 soft errors,  0 hard errors"`
set res2 = `cat ./logs/$log | awk '/^[ \t]*Total/ {print} /TablesTest/ {print}'`


set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res2"
 echo "$res2" | mail -s "Errors in hgTablesTestRobot on $HOST" ${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu
 exit 1
endif
#
echo Done.
echo "$res2" | mail -s "v${BRANCHNN} hgTablesTest robot done successfully on $HOST." ${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu
exit 0


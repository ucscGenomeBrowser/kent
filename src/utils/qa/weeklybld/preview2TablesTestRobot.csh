#!/bin/tcsh
cd $WEEKLYBLD

cd $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/hgTablesTest
make 
cd $WEEKLYBLD

@ NEXTNN = ( $BRANCHNN + 1 )

setenv HGDB_CONF /cluster/home/build/.hg.conf
set log = v${NEXTNN}.preview2.hgTables.log

echo "/cluster/bin/$MACHTYPE/hgTablesTest -db=hg38 https://hgwdev.gi.ucsc.edu/cgi-bin/hgTables ./logs/$log" > ./logs/$log
/cluster/bin/$MACHTYPE/hgTablesTest -appendLog -db=hg38 https://hgwdev.gi.ucsc.edu/cgi-bin/hgTables ./logs/$log
echo "" >> ./logs/$log
echo "/cluster/bin/$MACHTYPE/hgTablesTest -appendLog -org=Mouse -orgs=1  https://hgwdev.gi.ucsc.edu/cgi-bin/hgTables ./logs/$log" >> ./logs/$log
/cluster/bin/$MACHTYPE/hgTablesTest -appendLog -org=Mouse -orgs=1  https://hgwdev.gi.ucsc.edu/cgi-bin/hgTables ./logs/$log

# creates hgTables.log - look for unusual errors

#-- to check for errors: 
set res = `cat ./logs/$log | egrep "^[ \t]*Total" | egrep -v "0 soft errors,  0 hard errors"`
set res2 = `cat ./logs/$log | awk '/^[ \t]*Total/ {print} /TablesTest/ {print}'`


set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res2"
 echo "$res2" | mail -s "Errors in v${NEXTNN}.preview2 hgTablesTestRobot on $HOST" ${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu
 exit 1
endif
#
echo Done.
echo "$res2" | mail -s "v${NEXTNN}.preview2 hgTablesTest robot done successfully on $HOST." ${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu
exit 0


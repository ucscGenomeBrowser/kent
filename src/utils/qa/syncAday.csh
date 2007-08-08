#!/bin/tcsh

################################
#  
#  02-24-07
#  Robert Kuhn
#
#  runs sync on the database of the day.
#
################################



if ( $#argv != 1 ) then
  echo
  echo "  runs sync on the database of the day."
  echo "  replaces file in htdocs if run again same day."
  echo
  echo "    usage:  go"
  echo
  exit
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

set basePath='/usr/local/apache/htdocs/qa/test-results/sync'
set db=`databaseAday.csh today`
rm -f $basePath/$db
echo "\n$db\n" >> $basePath/$db
checkSync.csh $db times >> $basePath/$db
checkSync.csh $db hgw1 hgw2 times >> $basePath/$db
checkSync.csh $db hgw3 hgw4 times >> $basePath/$db
checkSync.csh $db hgw5 hgw6 times >> $basePath/$db
checkSync.csh $db hgw7 hgw8 times >> $basePath/$db
checkSync.csh $db hgwbeta mgc times >> $basePath/$db
cat $basePath/$db
echo "http://hgwdev.cse.ucsc.edu/qa/test-results/sync/$db" | mail -s "sync for today" $USER@soe.ucsc.edu


#!/bin/tcsh
source `which qaConfig.csh`

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
  echo "  checks for table sync on all machines for database of the day."
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

set basePath='/usr/local/apache/htdocs-genecats/qa/test-results/sync'
set db=`databaseAday.csh today`
rm -f $basePath/$db
echo "\n$db\n" >> $basePath/$db
# checkSync.csh $db times >> $basePath/$db
checkSync.csh $db hgwbeta hgw2 >> $basePath/$db
cat $basePath/$db
echo "http://genecats.soe.ucsc.edu/qa/test-results/sync/$db" 
echo 


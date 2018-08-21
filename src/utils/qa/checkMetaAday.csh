#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  02-24-07
#  Robert Kuhn
#
#  checks metadata for the database of the day.
#
################################



if ( $#argv != 1 ) then
  echo
  echo "  checks metadata for the database of the day."
  echo
  echo "    usage:  go"
  echo
  exit
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

set outPath='/usr/local/apache/htdocs-genecats/qa/test-results/metadata'
cd $outPath/details
rm -f $outPath/details/*
set db=`databaseAday.csh today`
rm -f $outPath/$db
checkMetaData.csh $db hgwbeta rr >> $outPath/$db
echo "\n   details in \
  http://genecats.soe.ucsc.edu/qa/test-results/metadata/details\n" \
  >> $outPath/$db
cat $outPath/$db 

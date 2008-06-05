#!/bin/tcsh

###############################################
# 
#  05-23-08
#  Robert Kuhn
#
#  finds any tables made since yesterday
# 
###############################################

set dbs=""
set yesterDate=""

if ($#argv == 0 || $#argv > 1) then
  # no command line args
  echo
  echo "  finds any tables made yesterday."
  echo
  echo "    usage:  database(s) "
  echo
  echo '      will take comma-separated list:  "hg18,mm9"'
  echo
  exit
else
  set dbs=`echo $argv[1] | sed "s/,/ /g"`
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n  error: you must run this script on dev!\n"
 exit 1
endif

set yesterDate=`date +%Y-%m-%d --date='1 day ago'`

echo
echo $yesterDate "new or updated tables"
echo

foreach db ( $dbs )
  echo $db
  getTableStatus.csh  $db hgwdev | awk '{print $1, $11}' | grep $yesterDate \
    | awk '{print $1}' | egrep -v "trackDb|hgFindSpec|tableDescriptions"
  echo
end



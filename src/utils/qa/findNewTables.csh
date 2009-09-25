#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  05-23-08
#  Robert Kuhn
#
#  finds any tables made since yesterday
# 
###############################################

set dbs=""
set yesterdate=""

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

# set yesterdate=2009-03-11
set yesterdate=`date +%Y-%m-%d --date='1 day ago'`

echo
echo $yesterdate "new or updated tables"
echo

foreach db ( $dbs )
  echo $db
  getTableStatus.csh  $db hgwdev | awk -F"\t" '{print $1, $13}' \
    | grep $yesterdate | awk '{print $1}' \
    | egrep -v "trackDb|hgFindSpec|tableDescriptions" | egrep -rv -f $GENBANK
  echo
end



#!/bin/tcsh

################################
#  updated:
#  04-08-04
#
################################

set tablelist=""
set db=""

if ($2 == "") then
  echo
  echo "  gets update times from dev and beta for tables in list." 
  echo "  if second argument is only a tablename, than a list of one value created."
  echo
  echo "    usage:  database, tablelist"
  echo
  exit
else
  set db=$1
  set tablelist=$2
endif
# cat $tablelist

if (! -e $tablelist) then
  echo $2 > list
  set tablelist="list"
endif

echo
echo "update times (dev first):"
echo
foreach table (`cat $tablelist`)
  echo $table
  echo "============="
  set dev=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $13, $14}'`
  set beta=`hgsql -h hgwbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $13, $14}'`
  echo "."$dev
  echo "."$beta
  echo
end
echo

rm -f list

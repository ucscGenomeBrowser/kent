#!/bin/tcsh

################################
#  
#  04-08-04
#  Robert Kuhn
#
#  gets update times from dev and beta for tables in list
#
################################

set tablelist=""
set db=""

if ($2 == "") then
  echo
  echo "  gets update times from dev and beta for tables in list." 
  echo
  echo "    usage:  database, tablelist (will accept single table)"
  echo
  exit
else
  set db=$1
  set tablelist=$2
endif
# cat $tablelist

if (! -e $tablelist) then
  echo $2 > xxtablelistxx
  set tablelist="xxtablelistxx"
endif

foreach table (`cat $tablelist`)
  echo
  echo $table
  echo "============="
  set dev=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $13, $14}'`
  set beta=`hgsql -h hgwbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $14, $15}'`
  echo "."$dev
  echo "."$beta
  echo
end

rm -f xxtablelistxx

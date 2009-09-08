#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  04-08-04
#  Robert Kuhn
#
#  gets update times from any two machines for tables in list
#
################################

set tablelist=""
set db=""
set first=""
set second=""
set third=""

if ( $#argv != 2 ) then
  echo
  echo "  gets update times for three machines for tables in list." 
  echo "  warning:  not in real time for RR.  uses overnight dump." 
  echo
  echo "    usage:  database tablelist "
  echo
  echo "            reports on dev, beta and RR"
  echo "            tablelist will accept single table"
  echo
  exit
else
  set db=$argv[1]
  set tablelist=$argv[2]
endif
# cat $tablelist

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif


# check if it is a file or a tablename
file $tablelist | egrep -q "ASCII text"
if (! $status) then
  set tables=`cat $tablelist`
else
  set tables=$tablelist
endif

foreach table ($tables)
  echo
  echo $table
  echo "============="

  set first=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $14, $15}'`
  if ( $status ) then
    echo "."
    continue
  endif

  set second=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $14, $15}'`
  if ( $status ) then
    echo "."
    continue
  endif

  set third=`getRRtableStatus.csh $db $table Update_time`
  if ( $status ) then
    set third=""
  endif

  echo "."$first
  echo "."$second
  echo
  echo "."$third
end
echo

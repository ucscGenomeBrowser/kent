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
set fourth=""

if ( $#argv != 2 ) then
  echo
  echo "  gets update times for three machines for tables in list." 
  echo "  if table is trackDb, trackDb_public will also be checked."
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

  if ( "$table" == "trackDb" ) then
    set third=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'trackDb_public'"' $db \
      | awk '{print $14, $15}'`
    if ( $status ) then
      echo "."
      continue
    endif
  endif

  set fourth=`getRRtableStatus.csh $db $table Update_time`
  if ( $status ) then
    set fourth=""
  endif

  echo "."$first
  echo "."$second
  if ( "$table" == "trackDb" ) then
    echo "."$third "(trackDb_public)"
  endif
  echo
  echo "."$fourth
end
echo

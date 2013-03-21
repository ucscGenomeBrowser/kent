#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  04-08-04
#  Robert Kuhn
#
#  gets update times from all machine levels for tables in list
#
################################

set tablelist=""
set db=""
set verbosity=0
set dot=( '.' '.' '.' '.' '.')
set first=""
set second=""
set third=""
set fourth=""
set fifth=""  # euro

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  gets update times for three machines for tables in list." 
  echo "  if table is trackDb, trackDb_public will also be checked."
  echo "  warning:  not in real time for RR.  uses overnight dump." 
  echo
  echo "    usage:  database tablelist [verbose]"
  echo
  echo "            reports on dev, beta, RR and euronode"
  echo "            tablelist will accept single table"
  echo "            verbose mode will print machines names"
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

if ( $#argv == 3 ) then
  if ( $argv[3] == "verbose" ) then
    set verbosity=1
    set dot=( 'dev  ' 'beta ' 'pub  ' 'rr   ' 'euro ' )
  else
    echo
    echo 'sorry. third argument must be "verbose"'
    $0
    exit
  endif
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
    echo "$dot[1]"
    continue
  endif

  set second=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $14, $15}'`
  if ( $status ) then
    echo "$dot[2]"
    continue
  endif

  if ( "$table" == "trackDb" ) then
    set third=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'trackDb_public'"' $db \
      | awk '{print $14, $15}'`
    if ( $status ) then
      echo "$dot[3]"
      continue
    endif
  endif

  set fourth=`getRRtableStatus.csh $db $table Update_time`
  if ( $status ) then
    set fourth=""
  endif

  set fifth=`getTableStatus.csh $db genome-euro | sed '1,2d' \
      | grep -w ^$table | awk '{print $14, $15}'`
  if ( $status ) then
    set fifth="."
  endif

  echo "$dot[1]"$first
  echo "$dot[2]"$second
  if ( "$table" == "trackDb" ) then
    echo "$dot[3]"$third "(trackDb_public)"
  endif
  echo
  echo "$dot[4]"$fourth
  echo "$dot[5]"$fifth
end
echo

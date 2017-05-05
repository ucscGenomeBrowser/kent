#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  10-10-2006
#  Robert Kuhn
#
#  gets update times from all machines in realTime
#
################################

set tablelist=""
set tables=""
set db=""
set verbosity="0"
set dot=( '.' '.' '.' '.' '.' )
set update=""

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  gets update times from all machines in real time for tables in list." 
  echo
  echo "    usage:  database table/list [verbose]"
  echo
  echo " output:"
  echo " . hgwdev"
  echo " . hgwbeta"
  echo
  echo " . hgw6"
  echo " . genome-euro"
  echo " . genome-asia"
  echo
  exit
else
  set db=$argv[1]
  set tablelist=$argv[2]
endif
# cat $tablelist


if ( $#argv == 3 ) then
  if ( $argv[3] == "verbose" ) then
    set verbosity=1
    set dot=( 'dev ' 'beta' 'rr  ' 'euro' 'asia')
  else
    echo
    echo 'sorry. third argument must be "verbose"'
    $0
    exit
  endif
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

# check if it is a file or a tablename
file $tablelist | egrep "ASCII text" > /dev/null
if (! $status) then
  set tables=`cat $tablelist`
else
  set tables=$tablelist
endif

foreach table ($tables)
  echo
  echo $table
  echo "============="
  set i=1
  foreach machine ( hgwdev hgwbeta hgw6 genome-euro genome-asia )
    if ( "hgw6" == $machine ) then
      echo  # space out results
    endif
    set update=`getTableStatus.csh $db $machine | sed '1,2d' \
      | grep -w ^$table | awk '{print $14, $15}'`
    if ( $status ) then
      echo "$dot[$i]"
      @ i = $i + 1
      continue
    endif
    echo "$dot[$i] "$update
    @ i = $i + 1
  end
end
echo

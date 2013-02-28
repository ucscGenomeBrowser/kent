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
set dot=( '.' '.' '.' '.' )
set ver=""
set subver=""
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
  echo " . hgw8"
  echo " . genome-euro"
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
    set dot=( 'dev ' 'beta' 'rr  ' 'euro' )
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
  foreach machine ( hgwdev hgwbeta hgw8 genome-euro )
    if ( "hgw8" == $machine ) then
      echo  # space out results
    endif
    # find out version of mysql running 
    # (v 5 has different signature for TABLE STATUS output)
    # (so does ver 4.1.*)
    set ver=`getVersion.csh $machine 1` >& /dev/null
    set subver=`getVersion.csh $machine 2`
    if ( 4 == $ver && 1 == $subver || 5 == $ver ) then
      # newer mysql versions use different fields
      set update=`getTableStatus.csh $db $machine | sed '1,2d' \
        | grep -w ^$table | awk '{print $14, $15}'`
      if ( $status ) then
        echo "$dot[$i]"
        @ i = $i + 1
        continue
      endif
    else
      set update=`getTableStatus.csh $db $machine | sed '1,2d' \
        | grep -w ^$table | awk '{print $13, $14}'`
      if ( $status ) then
        echo "$dot[$i]"
        continue
      endif
    endif
    echo "$dot[$i] "$update
    @ i = $i + 1
  end
end
echo

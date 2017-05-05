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
set dot=( '. ' '. ' '. ' '. ' '. ' '. ')
set first=""
set second=""
set third=""
set fourth=""
set noMirror=false
set fifth=""  # euro
set sixth=""  # asia

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  gets update times for three machines for tables in list." 
  echo "  if table is trackDb, trackDb_public will also be checked."
  echo "  warning:  not in real time for RR.  uses overnight dump." 
  echo
  echo "    usage:  database tablelist [verbose | noMirror | verboseNoMirror]"
  echo
  echo "            reports on dev, beta, RR, genome-euro and genome-asia"
  echo "            tablelist will accept single table"
  echo "            verbose mode will print machines names"
  echo "            noMirror mode will stop before checking genome-euro and genome-asia"
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
  if ( $argv[3] == "noMirror" || $argv[3] == "verboseNoMirror" ) then
    set noMirror=true
  endif
  if ( $argv[3] == "verbose" || $argv[3] == "verboseNoMirror" ) then
    set dot=( 'dev  ' 'beta ' 'pub  ' 'rr   ' 'euro ' 'asia ')
  else
    if ( $noMirror == "false" ) then
      echo '\nsorry. third argument must be "verbose" or "noMirror" or "verboseNoMirror"'
      $0
      exit
    endif
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
  echo "$dot[1]"$first

  set second=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $14, $15}'`
  echo "$dot[2]"$second

  if ( "$table" == "trackDb" ) then
    set third=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'trackDb_public'"' $db \
      | awk '{print $14, $15}'`
    echo "$dot[3]"$third  "(trackDb_public)"
  endif
  echo

  set fourth=`getRRtableStatus.csh $db $table Update_time`
  if ( $status ) then
    set fourth=""
  endif
  echo "$dot[4]"$fourth

  if ( $noMirror == "false" ) then
    set fifth=`getTableStatus.csh $db genome-euro | sed '1,2d' \
        | grep -w ^$table | awk '{print $14, $15}'`
    if ( $status ) then
      set fifth=""
    endif
    echo "$dot[5]"$fifth

    set sixth=`getTableStatus.csh $db genome-asia | sed '1,2d' \
        | grep -w ^$table | awk '{print $14, $15}'`
    if ( $status ) then
      set sixth=""
    endif
    echo "$dot[6]"$sixth
  endif

end
echo

#!/bin/tcsh

################################
#  
#  10-10-06
#  Robert Kuhn
#
#  gets update times from all machines in realTime
#
################################

# single indent means not changed, but checked.
# multi-indent means changed
set tablelist=""
set db=""
set mach1="hgwdev"
set mach2="hgwbeta"
    set mach3="hgw1"
set host1=""
    set host2="-h hgwbeta"
 set rr1="false"
 set rr2="false"
    set rr3="true"
set first=""
set second=""
set third=""
    set allThree="all"

if ( $#argv != 2 ) then
  echo
  echo "  gets update times from all machines in real time for tables in list." 
  echo
  echo "    usage:  database tablelist (will accept single table)"
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

# make a file for single-table queries
file $tablelist | egrep "ASCII text" > /dev/null
if ($status) then
  echo $argv[2] > xxtablelistxx
  set tablelist="xxtablelistxx"
endif

foreach table (`cat $tablelist`)
  echo
  echo $table
  echo "============="

  set first=`hgsql $host1 -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $13, $14}'`
  set second=`hgsql $host2 -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
    | awk '{print $13, $14}'`
  echo "."$first
  echo "."$second
  echo

  foreach machine (hgw1 hgw2 hgw3 hgw4 hgw5 hgw6 hgw7 hgw8 mgc)
    # find out version of mysql running 
    # (v 5 has different signature for TABLE STATUS output)
    set sqlVersion=`ssh -x qateam@$machine mysql $db -A -N -e '"'SELECT @@version'"' \
      | awk -F. '{print $1}'`
    if (4 == $sqlVersion) then
      set third=`ssh -x qateam@$machine mysql $db -A -N -e '"'SHOW TABLE STATUS'"' \
        | grep -w ^$table | awk '{print $11, $12}'`
    else
      set third=`ssh -x qateam@$machine mysql $db -A -N -e '"'SHOW TABLE STATUS'"' \
        | grep -w ^$table | awk '{print $12, $13}'`
    endif
    if ("mgc" == $machine ) then
      echo
    endif
    echo "."$third
  end
end
echo

rm -f xxtablelistxx

#!/bin/tcsh

################################
#  
#  10-10-06
#  Robert Kuhn
#
#  gets update times from all machines in realTime
#
################################

set tablelist=""
set db=""
set host1=""
set host2="-h hgwbeta"
set sqlVersion=0
set sqlSubVersion=0
set first=""
set second=""
set third=""

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

  foreach machine (hgwdev hgwbeta hgw1 hgw2 hgw3 hgw4 hgw5 hgw6 hgw7 hgw8 mgc)
    # find out version of mysql running 
    # (v 5 has different signature for TABLE STATUS output)
    # (so does ver 4.1.*)
    set sqlVersion=`ssh -x qateam@$machine mysql $db -A -N \
      -e '"'SELECT @@version'"' | awk -F. '{print $1}'`
    set sqlSubVersion=`ssh -x qateam@$machine mysql $db -A -N \
      -e '"'SELECT @@version'"' | awk -F. '{print $2}'`
    # echo $sqlVersion $sqlSubVersion
    if (4 == $sqlVersion && 0 == $sqlSubVersion) then
      set third=`ssh -x qateam@$machine mysql $db -A -N \
        -e '"'SHOW TABLE STATUS'"' | grep -w ^$table | awk '{print $13, $14}'`
    else
      set third=`ssh -x qateam@$machine mysql $db -A -N \
        -e '"'SHOW TABLE STATUS'"' | grep -w ^$table | awk '{print $14, $15}'`
    endif
    if ("mgc" == $machine || "hgw1" == $machine ) then
      echo
    endif
    echo "."$third
  end
end
echo

rm -f xxtablelistxx

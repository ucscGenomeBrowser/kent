#!/bin/tcsh

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
set mach1="hgwdev"
set mach2="hgwbeta"
set host1=""
set host2=""
set rr1="false"
set rr2="false"

if ( $#argv != 2 && $#argv != 4 ) then
  echo
  echo "  gets update times from any two machines for tables in list." 
  echo
  echo "    usage:  database, tablelist (will accept single table),"
  echo "            [machine1], [machine2] (defaults to dev and beta)"
  echo
  exit
else
  set db=$argv[1]
  set tablelist=$argv[2]
endif
# cat $tablelist

if ($#argv == 4) then
  set mach1=$argv[3]
  set mach2=$argv[4]
endif

# confirm that machine names are legit
checkMachineName.csh $mach1
if ( $status ) then
  exit 1
endif

checkMachineName.csh $mach2
if ( $status ) then
  exit 1
endif

# set host names for beta queries
if ( $mach1 == "hgwbeta" ) then
  set host1="-h hgwbeta"
endif

if ( $mach2 == "hgwbeta" ) then
  set host2="-h hgwbeta"
endif

# set flags for RR queries
if ( $mach1 != "hgwdev" &&  $mach1 != "hgwbeta" ) then
  set rr1="true"
endif

if ( $mach2 != "hgwdev" &&  $mach2 != "hgwbeta" ) then
  set rr2="true"
endif

# make a file for single-table queries
if (! -e $tablelist) then
  echo $argv[2] > xxtablelistxx
  set tablelist="xxtablelistxx"
endif


foreach table (`cat $tablelist`)
  echo
  echo $table
  echo "============="

  if ( $rr1 == "true" ) then
    set first=`getRRtableStatus.csh $db $table update_time`
  else
    set first=`hgsql $host1 -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
      | awk '{print $13, $14}'`
  endif

  if ( $rr2 == "true" ) then
    set second=`getRRtableStatus.csh $db $table update_time`
  else
    set second=`hgsql $host2 -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
      | awk '{print $13, $14}'`
  endif

  echo "."$first
  echo "."$second
  echo
end

rm -f xxtablelistxx

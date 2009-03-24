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
set mach3=""
set host1=""
set host2=""
set rr1="false"
set rr2="false"
set rr3="false"
set first=""
set second=""
set third=""
set allThree=""

if ( $#argv < 2 || $#argv > 5 ) then
  echo
  echo "  gets update times from any two or all three machines for tables in list." 
  echo "  warning:  not in real time for RR.  uses overnight dump." 
  echo
  echo "    usage:  database tablelist (will accept single table),"
  echo "            [3 | all | machine1] [machine2] [machine3] "
  echo "            (defaults to dev and beta)"
  echo
  echo '            "3" indicates dev, beta and hgw1'
  echo '            "all" indicates dev, beta, all 8 RR machines'
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


if ($#argv == 3) then
  set allThree=$argv[3]
  if ($allThree == 3 || $allThree == "all") then
    set rr1="false"
    set rr2="false"
    set rr3="true"
    set mach3="hgw1"
  else
    set mach1=$argv[3]
    set mach2=""
    checkMachineName.csh $mach1
    if ($status) then
      exit 1
    endif
  endif
endif

if ( $#argv == 4  || $#argv == 5) then
  set mach1=$argv[3]
  set mach2=$argv[4]
  if ( $#argv == 5) then
    set mach3=$argv[5]
  endif
  checkMachineName.csh $mach1 $mach2 $mach3
  if ( $status ) then
    exit 1
  endif
endif

if ( $mach1 == "hgwbeta" ) then
  set host1="-h hgwbeta"
endif

if ( $mach2 == "hgwbeta" ) then
  set host2="-h hgwbeta"
endif
 
if ( $mach3 == "hgwbeta" ) then
  set host3="-h hgwbeta"
endif

# set flags for RR queries
if ( $mach1 != "hgwdev" &&  $mach1 != "hgwbeta" ) then
  set rr1="true"
endif

if ( $mach2 != "hgwdev" &&  $mach2 != "hgwbeta" ) then
  set rr2="true"
endif

if ( $mach3 != "hgwdev" &&  $mach3 != "hgwbeta" ) then
  set rr3="true"
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

  if ( $rr1 == "true" ) then
    set first=`getRRtableStatus.csh $db $table update_time $mach1`
  else
    set first=`hgsql $host1 -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
      | awk '{print $13, $14}'`
  endif

  if ( $rr2 == "true" ) then
    set second=`getRRtableStatus.csh $db $table update_time $mach2`
  else
    set second=`hgsql $host2 -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
      | awk '{print $13, $14}'`
  endif

  if ( $mach3 != "" ) then
    if ( $rr3 == "true" ) then
      set third=`getRRtableStatus.csh $db $table update_time $mach3`
    else
      set third=`hgsql $host3 -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
        | awk '{print $13, $14}'`
    endif
  endif

  echo "."$first
  if ( $mach2 != "" ) then
    echo "."$second
  endif
  if ($allThree == 3 || ($mach3 != "" && $allThree != "all")) then
    echo "."$third
  else
    echo
  endif

  if ($allThree == "all") then
    foreach machine ( hgw1 hgw2 hgw3 hgw4 hgw5 hgw6 hgw7 hgw8 )
      set third=`getRRtableStatus.csh $db $table update_time $machine`
      echo "."$third
    end
  endif
end
echo

#!/bin/tcsh

################################
#  
#  01-24-07
#  Robert Kuhn
#
#  gets size of table from TABLE STATUS dumps
#
################################

set tableinput=""
set tables=""
set db=""
set list=""
set first=""
set second=""
set machine1="hgwbeta"
set machine2=""
set mach1Tot=0
set mach2Tot=0

if ( $#argv < 2 || $#argv > 4 ) then
  echo
  echo "  gets size of table from TABLE STATUS dumps"
  echo
  echo "    usage:  database tablelist [machine1] [machine2]"
  echo "           tablelist may be single table"
  echo "           defaults to hgwbeta"
  echo "           uses overnight STATUS dumps.  not real time"
  echo
  exit
else
  set db=$argv[1]
  set tableinput=$argv[2]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

# set machine name2 and check validity
if ( $#argv > 2 ) then
  set machine1="$argv[3]"
  checkMachineName.csh $machine1
  if ( $status ) then
    exit 1
  endif
endif
if ( $#argv == 4 ) then
  set machine2="$argv[4]"
  checkMachineName.csh $machine2
  if ( $status ) then
    exit 1
  endif
endif

# check if it is a file or a tablename and set list
file $tableinput | egrep "ASCII text" > /dev/null
if ( ! $status ) then
  set tables=`cat $tableinput`
  set list="true"
else
  set tables=$tableinput
endif

foreach table ($tables)
  echo
  echo $table
  echo "============="
  set first=`getRRtableStatus.csh $db $table Data_length $machine1 \
    | awk '{printf("%0.2f", $1/1000000) }'`
  echo "$first megabytes"
  if ( "" != $machine2) then
    set second=`getRRtableStatus.csh $db $table Data_length $machine2 \
      | awk '{printf("%0.2f", $1/1000000) }'`
    echo "$second megabytes"
  endif 
  set mach1Tot=`echo $mach1Tot $first  | awk '{print $1+$2}'`
  set mach2Tot=`echo $mach2Tot $second | awk '{print $1+$2}'`
end 

# output totals if more than one table
echo
if ( "true" == $list ) then
  echo $machine1 "total" "=" $mach1Tot \
    | awk '{printf("%7s %5s %1s %0.2f\n", $1, $2, $3, $4)}'
  if ( $machine2 != "" ) then
    echo $machine2 "total" "=" $mach2Tot \
      | awk '{printf("%7s %5s %1s %0.2f\n", $1, $2, $3, $4)}'
  endif
  echo
endif 


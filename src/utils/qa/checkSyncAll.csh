#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  11-17-06
#  Robert Kuhn
#
#  checks all dbs between two nodes in realTime
#
################################

set mach1="hgwbeta"
set mach2="hgw1"
set databases=""
set times=""

if ( $#argv < 1 || $#argv > 4 ) then
  echo
  echo "  checks table match for all dbs between two nodes in realTime"
  echo "  ignores genbank tables"
  echo "  uses db list from first machine"
  echo "  optionally reports if update times do not match."
  echo
  echo "    usage:  go [machine1 machine2] [times]"
  echo "              defaults to beta and hgw1"
  echo
  exit
else
  if ( "go" != $argv[1] ) then
    echo
    echo ' the first argument must be "go"\n'
    echo ${0}:
    $0
    exit 1
  endif
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif


if ( $#argv == 2 ) then
  if ( $argv[2] == "times" ) then
    set times="times"
  else
    echo
    echo " you must specify two machines if you specify one"
    echo
    echo ${0}:
    $0
    exit 1
  endif
endif

if ( $#argv == 3 || $#argv == 4 ) then
  set mach1=$argv[2]
  set mach2=$argv[3]
endif

checkMachineName.csh $mach1 $mach2
if ($status) then
  exit 1
endif

if ( $#argv == 4 ) then
  if ( $argv[4] == "times" ) then
    set times="times"
  else
    echo
    echo ' the fourth argument must be "times"\n'
    echo ${0}:
    $0
    exit 1
  endif
endif


set databases=`ssh -x qateam@$mach1 mysql -A -N \
  -e '"'SHOW DATABASES'"'`

foreach db ( $databases )
  echo $db
  echo
  checkSync.csh $db $mach1 $mach2 $times
  echo "=========================="
  echo
end


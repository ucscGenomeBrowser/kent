#!/bin/tcsh
source `which qaConfig.csh`


#######################
#
#  01-07-2005
#  checks if machine name is legitimate.
#  gets the names of all databases that contain a given table
#  
#  Robert Kuhn
# 
#######################

if ($#argv < 1 || $#argv > 3) then
 echo ""
 echo "  checks if machine names are legitimate."
 echo "    no output if machine is ok."
 echo
 echo "    usage: machinename [machine2] [machine3]"
 echo ""
 exit 1
endif

set allMachines=" hgwdev hgwbeta hgw1 hgw2 hgw3 hgw4 hgw5 hgw6 hgw7 hgw8 rr RR genome-euro genome-asia"

set mach1 = $argv[1]
echo $allMachines | grep -w "$mach1" > /dev/null
if ($status) then
  echo "\n $mach1 is not a valid machine.\n"
  exit 1
endif

if ($#argv > 1) then
  set mach2 = $argv[2]
  echo $allMachines | grep -w "$mach2" > /dev/null
  if ($status) then
    echo "\n $mach2 is not a valid machine.\n"
    exit 1
  endif
endif

if ($#argv == 3) then
  set mach3 = $argv[3]
  echo $allMachines | grep -w "$mach3" > /dev/null
  if ($status) then
    echo "\n $mach3 is not a valid machine.\n"
    exit 1
  endif
endif


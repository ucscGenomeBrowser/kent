#!/bin/tcsh


#######################
#
#  01-02-2005
#  checks if machine name is legitimate.
#  gets the names of all databases that contain a given table
#  
#  Robert Kuhn
# 
#######################

if ( $#argv != 1 ) then
 echo ""
 echo "  checks if machine name is legitimate."
 echo "    no output if machine is ok."
 echo
 echo "    usage: machinename"
 echo ""
 exit 1
endif

set machine1 = $argv[1]

set allMachines=" hgwdev hgwbeta hgw1 hgw2 hgw3 hgw4 hgw5 hgw6 hgw7 hgw8 mgc "

echo $allMachines | grep -w "$machine1" > /dev/null
if ($status) then
  echo "\n $machine1 is not a valid machine.\n"
  exit 1
endif


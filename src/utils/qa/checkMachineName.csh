#!/bin/tcsh

if ( $#argv != 1 ) then
 echo ""
 echo "  checks if machine name is legitimate."
 echo
 echo "    usage: $0 machinename"
 echo ""
 exit 1
endif

set machine1 = $argv[1]

set allMachines=" hgwdev hgwbeta hgw1 hgw2 hgw3 hgw4 hgw5 hgw6 hgw7 hgw8 mgc "

set mach1Ok=`echo $allMachines | grep -w "$machine1"`
if ($status) then
  echo "\n  $machine1 is not a valid machine.\n"
  exit 1
endif


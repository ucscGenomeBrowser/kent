#!/bin/tcsh
source `which qaConfig.csh`

################################
#  04-09-04 (revised 5/20/09, brooke)
#
#  Prints the chrom names for an assembly.
#
################################

set db=""
set norandom=""

if ($#argv < 1 || $#argv >2 ) then
  echo
  echo "  prints the chrom names for an assembly."
  echo
  echo "    usage:  database [norandom]"
  echo
  exit
else
  set db=$1
endif

if ( $#argv == 2 ) then
  if ( $argv[2] == "norandom" ) then
    set norandom="true"
  else
    echo
    echo ' second parameter can only be "norandom"'
    echo
    exit 1
  endif
endif

if ($norandom == "true") then
  hgsql -N -e "SELECT chrom FROM chromInfo" $db | grep -v "random" | sort
else
  hgsql -N -e "SELECT chrom FROM chromInfo" $db | sort
endif

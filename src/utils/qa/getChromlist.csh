#!/bin/tcsh


################################
#  04-09-04
#
#  Writes a file with the chromnames for that assembly.
#
################################

set db=""
set norandom=""

if ($#argv < 1 || $#argv >2 ) then
  echo
  echo "  writes a file with the chromnames for an assembly."
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

# echo "list chroms"

if ($norandom == "true") then
  hgsql -N -e "SELECT chrom FROM chromInfo" $db | grep -v "random" | sort \
    > $db.chromlist
else
  hgsql -N -e "SELECT chrom FROM chromInfo" $db | sort \
    > $db.chromlist
endif

cat $db.chromlist

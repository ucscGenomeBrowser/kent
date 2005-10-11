#!/bin/tcsh

#######################
#
#  08-06-05
#  checks all /cluster/store units and reports space avail
#
#######################

if ($#argv < 1 || $#argv > 1) then
  echo
  echo "  checks all /cluster/store units and reports space avail."
  echo
  echo "    usage:  go"
  echo
  exit
endif

df -kh | grep store | egrep % | sort -k4 -r


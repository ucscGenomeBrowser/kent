#!/bin/tcsh

#######################
#
#  08-29-05
#  checks precesses running on RR
#
#######################
  
set go=""

if ($#argv < 1 || $#argv > 1) then
  echo
  echo " gets the list of processes on RR and checks to see which "
  echo "    nodes are being used"
  echo
  echo "    usage:  go"
  echo
  exit
else
  set go=$argv[1]
endif

# assign command line arguments
if ($go != "go") then
  echo
  echo "  $0"
  echo
  $0
  exit 1
endif

hgsql -N -h genome-centdb -e "SHOW PROCESSLIST" hgcentral | awk '{print $3}' \
 | awk -F"." '{print $1}' | sort | uniq -c | grep -v hgwdev



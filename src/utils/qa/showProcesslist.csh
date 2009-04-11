#!/bin/tcsh
source `which qaConfig.csh`

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

echo "\nprocesses:\n"
hgsql -N -h genome-log -e "SHOW PROCESSLIST" hgcentral | awk '{print $3}' \
 | awk -F"." '{print $1}' | sort | uniq -c | grep -v hgwdev

echo "\nhits in last 10 min:\n"

set max=`hgsql -N -h genome-log -e "SELECT MAX(time_stamp) FROM access_log" apachelog` 
hgsql -N -h genome-log -e "SELECT DISTINCT(machine_id), COUNT(*) FROM access_log \
  WHERE time_stamp > $max - 600 GROUP BY machine_id " apachelog | awk '{printf("%7s %5d\n"), $1, $2}'
echo

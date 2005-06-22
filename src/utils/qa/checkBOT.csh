#!/bin/tcsh

####################
#  06-15-05 Bob Kuhn
#
#  Wrapper around bottleneck check
#
####################

set worst=""

if ($#argv != 1) then
  echo
  echo "  wrapper around bottleneck check"
  echo
  echo '      usage:  ipAddress '
  echo
  echo '  (will use "all" to get all IPs having delays)'
  echo
  exit
else
  set ip=$argv[1]
endif

echo
if ($ip == "all") then
  rm -f ipFile
  ssh hgwbeta /etc/init.d/bottleneck status | egrep "current"
  ssh hgwbeta /etc/init.d/bottleneck status | grep -w -v "0" \
     | grep -v "current" | sort -nr -k5
  set worst=`ssh hgwbeta /etc/init.d/bottleneck status | grep -w -v "0" \
     | grep -v "current" | sort -nr -k5 | head -1 | awk '{print $1}'`
else
  ssh hgwbeta /etc/init.d/bottleneck status | egrep "$ip|current"
endif


echo 
echo "  hits    = total number of accesses since started tracking"
echo "  time    = last seen hit was N seconds ago"
echo "  max     = the most delay time slapped on this source IP"
echo "  current = the current delay in miliseconds."
echo
echo "  delay decays at the rate of 10 miliseconds per second"
echo
echo "  current shows the number of milliseconds they would "
echo "  be delayed if they hit us with a query now.  "
echo "  If current is over 10,000 they get a message.  If it's"
echo "  over 15,000 they get cut off with a ruder message."
echo


echo "  the longest delay is from:"

if ($worst != "") then
  ipw $worst | head -10
endif

rm -f ipFile
exit

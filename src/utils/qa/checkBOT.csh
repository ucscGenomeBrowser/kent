#!/bin/tcsh

####################
#  06-15-05 Bob Kuhn
#
#  Wrapper around bottleneck check
#
####################

set ip=""
set worst=""
set mode=""

if ($#argv < 1 || $#argv > 2 ) then
  echo
  echo "  wrapper around bottleneck check."
  echo "  gives delay stats for IP address(es)."
  echo
  echo '      usage:  ipAddress [terse]'
  echo '              (terse gives only data)'
  echo
  echo '      (use ipAddress = "all" to get all IPs having delays)'
  echo
  exit
else
  set ip=$argv[1]
endif

if ($#argv == 2 ) then
  set mode=$argv[2]
  if ($mode !=  "terse" ) then
    echo
    echo '  sorry, the second argument can only be "terse"'
    echo
    exit 1
  endif
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

echo
if ($ip == "all") then
  rm -f ipFile
  /usr/local/bin/bottleneck -host=genome-bottle list | egrep "current"
  /usr/local/bin/bottleneck -host=genome-bottle list | grep -w -v "0" \
     | grep -v "current" | sort -nr -k5 > ipFile
  set allIPs=`cat ipFile | awk '{print $1}'`
  set worst=`echo $allIPs | awk '{print $1}'`
  cat ipFile
  echo

  foreach ip (`echo $allIPs`)
    set orgName=`ipw $ip | grep OrgName | sed -e "s/OrgName: //"` > /dev/null
    set current=`grep -w $ip ipFile | awk '{print $5}'`
    echo "$ip\t\t$current\t  $orgName"
  end
else
  /usr/local/bin/bottleneck -host=genome-bottle list | egrep -w "$ip|current"
endif
echo 

if ($mode == "terse") then
  exit 0
endif

echo "  hits    = total number of accesses since started tracking"
echo "  time    = last seen hit was N seconds ago"
echo "  max     = the most delay time slapped on this source IP"
echo "  current = the current delay in milliseconds."
echo
echo "  delay decays at the rate of 10 milliseconds per second"
echo '  each new hit adds 150 milliseconds to "current"'
echo
echo "  current shows the number of milliseconds they would "
echo "  be delayed if they hit us with a query now.  "
echo "  If current is over 10,000 they get a message.  If it's"
echo "  over 15,000 they get cut off with a ruder message."
echo

echo
echo "the longest delay is from:"

if ($worst != "") then
  ipw $worst | head -10
endif

rm -f ipFile
exit

#!/bin/tcsh
source `which qaConfig.csh`

####################
#  06-15-05 Bob Kuhn
#
#  Wrapper around bottleneck check
#
####################

onintr cleanup

set ip=""
set chopIDs=""
set worst=""
set mode=""
set bottleHost="hgnfs1"
# host is specified in /usr/local/apache/cgi-bin/hg.conf (as bottleneck.host)

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

echo
if ($ip == "all") then
  ssh qateam@hgwbeta bottleneck -host=$bottleHost list | egrep "current"
  ssh qateam@hgwbeta bottleneck -host=$bottleHost list | grep -w -v "0" \
     | grep -v "current" | sort -nr -k5 > ipFile$$
  set allIPs=`cat ipFile$$ | awk '{print $1}'`
  cat ipFile$$
  echo
  # get locations (strip off sessionID)
  set chopIPs=`echo $allIPs | sed "s/ /\n/"g \
    | awk -F"." '{print $(NF-3)"."$(NF-2)"."$(NF-1)"."$NF}'`
  set worst=`echo $chopIPs | awk '{print $1}'`
  foreach address ( $chopIPs )
    set orgName=`whois $address | grep OrgName | sed -e "s/OrgName: //"` > /dev/null
    set current=`grep -w $address ipFile$$ | awk '{print $5}'`
    echo "$address\t\t$current\t  $orgName"
  end
else
  ssh qateam@hgwbeta bottleneck -host=$bottleHost list | egrep -w "$ip|current"
endif
echo 

if ($mode == "terse") then
  exit 0
endif

echo "  hits    = total number of accesses since started tracking"
echo "  time    = seconds since last hit"
echo "  max     = the most delay time slapped on this source IP, in ms"
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

if ($ip == "all") then
  echo
  echo "partial whois info for IP with longest delay:"
  echo
  if ($worst != "") then
    whois $worst | head -15
  endif
endif

cleanup:
rm -f ipFile$$
exit

#!/bin/tcsh

####################
#  08-28-07 Bob Kuhn
#
#  Allow selected IP(s) to avoid slowdown by BOTtleneck server."
#
####################

set debug=0
set ip=""
set duration=0
set startField=""
set lastField=""
set threeFields=""
set i=0
set now=""

if ($#argv < 2 || $#argv > 3 ) then
  echo
  echo "  allow selected IP(s) to avoid slowdown by BOTtleneck server."
  echo
  echo '      usage:  ipAddress time [endOfRange]'
  echo
  echo '              (ipAddress also accepts file of ipAddresses)'
  echo '              (time in minutes to keep IP alive)'
  echo '              (endOfRange value should be last field of IP)'
  echo
  echo '      sets delay to zero every 30 sec -- hardcoded'
  echo
  exit
else
  set ip=$argv[1]
  set duration=$argv[2]
endif

if ( $#argv == 3 ) then
  set lastField=$argv[3]
endif

# setting up for multiple IPs
# check if IP is a file or a tablename
file $ip | egrep -q "ASCII text" 
if (! $status) then
  # there is a file
  set ip=`cat $ip`
else
  # get range of IPs if given in that format
  # get last field of first IP address
  set startField=`echo $ip | awk -F. '{print $NF}'` 
  set threeFields=`echo $ip | sed -e 's/'${startField}'$//'`
  if ( $lastField != ""  && $startField > $lastField ) then
    echo
    echo "  sorry, $lastField is not greater than last field of $ip."
    echo
    echo $0
    $0
    exit 1
  endif
  set i=$startField
  #set up all IPs in range
  while ( $i < $lastField )
    set i=`echo $i | awk '{print $1+1}'`
    set ip=`echo $ip $threeFields$i`
    echo
  end
endif

if ( $debug == 1 ) then
  echo "ip $ip"
  echo "duration $duration"
  echo "startField $startField"
  echo "lastField $lastField"
  echo "threeFields $threeFields"
endif

# set up times for keeping IPs alive for designated time 
set startTime=`date +%s`
set endTime=`echo $startTime $duration | awk '{print $1+$2*60}'`

echo
while ( `date +%s` < $endTime )
  foreach address ( $ip )
    /usr/local/bin/bottleneck -host=genome-bottle set $address 0 \
      | egrep .
    checkBOT.csh $address terse | egrep .
  end
  echo
  sleep 30
end


#!/bin/tcsh

####################
#  08-28-07 Bob Kuhn
#
#  Allow selected IP(s) to avoid slowdown by BOTtleneck server."
#
####################

set debug=0
set sleep=10
set ip=""
set duration=0
set startField=""
set lastField=""
set threeFields=""
set i=0

if ($#argv < 2 || $#argv > 4 ) then
  echo
  echo "  allow selected IP(s) to avoid slowdown by BOTtleneck server."
  echo '  resets delay to zero every n seconds for each IP.' 
  echo
  echo '      usage:  ipAddress time [endOfRange] [sleep=n]'
  echo
  echo '              (ipAddress also accepts file of ipAddresses)'
  echo '              (time in minutes to keep IP alive)'
  echo '              (endOfRange value should be last field of IP)'
  echo '              (sleep time in seconds for each IP - default = 10)'
  echo
  exit
else
  set ip=$argv[1]
  set duration=$argv[2]
endif

# set up optional arguments.  
if ( $#argv > 2 ) then
  echo $argv[3] | grep sleep > /dev/null
  if ( $status ) then
    set lastField=$argv[3]
  else
    set sleep=` echo $argv[3] | sed "s/sleep=//"`
  endif
  if ( $#argv == 4 ) then
    set sleep=` echo $argv[4] | sed "s/sleep=//"`
  endif
endif

if ( $debug == 1 ) then
  echo "lastField = $lastField"
  echo "sleep     = $sleep"
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
  set lastField=`echo $lastField | sed -e 's/^'${threeFields}'//'`
  if ( $debug == 1 ) then
    echo "ip $ip"
    echo "duration $duration"
    echo "startField $startField"
    echo "lastField $lastField"
    echo "threeFields $threeFields"
  endif
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

# resetting
echo
while ( `date +%s` < $endTime )
  date
  foreach address ( $ip )
    echo
    checkBOT.csh $address terse | egrep . | grep -v hits
    /usr/local/bin/bottleneck -host=genome-bottle set $address 0 \
      | egrep .
    checkBOT.csh $address terse | egrep .
  end
  echo "--------------------"
  echo
  sleep $sleep
end


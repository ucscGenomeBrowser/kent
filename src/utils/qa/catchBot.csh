#!/bin/tcsh

####################
#  07-09-04 Bob Kuhn
#
#  Script to catch bots.
#
####################

set threshhold=10000  # the number of hits from a user to use as the alarm level
set size=5            # the number of users to check
set db=""
set output="xxResultsxx"
set hourLimit = 1000

if ($#argv < 1 || $#argv > 2) then
  # not enough command line args
  echo
  echo "  script to catch bots."
  echo "  monitors the IP addresses of most common users."
  echo "    and squawks when someone gets too high."
  echo "  also checks the last hour."
  echo
  echo "    usage:  threshhold [report size] "
  echo "      where threshhold is number of hits from single source"
  echo "      report size is number of highest hitters listed (defaults to 5)"
  echo
  exit
else
  set threshhold=$argv[1]
endif

if ($#argv == 2) then
  set size=$argv[2]
endif

set debug="false"
# get some times
set lastHit=`hgsql -N -h genome-log -e 'SELECT MAX(time_stamp) FROM access_log' apachelog`
set firstHit=`hgsql -N -h genome-log -e 'SELECT MIN(time_stamp) FROM access_log' apachelog`
set hitSpan=`echo $lastHit $firstHit | gawk '{printf "%.1f", ($1 - $2) / 3600}'`
set hourAgo=`echo $lastHit | gawk '{printf "%.0f", $1 - 3600}'`

if ($debug == "true") then
  echo "lastHit  = $lastHit (seconds)"
  echo "firstHit = $firstHit (seconds)"
  echo "hitSpan  = $hitSpan (hours)"
  echo "hourAgo  = $hourAgo (sec) (number of epochseconds an hour ago)"
endif

# get whole list from access_log
#### 
hgsql -N -h genome-log -e "SELECT remote_host FROM access_log" apachelog > remote_host
#### 
sort remote_host | uniq -c | sort -nr > xxUserCountxx
set totalHits=`wc -l remote_host | gawk '{print $1}'`

set checked=0
set max=0

# write header to file
rm -f $output
echo "\nfrom apachelogs.access_log covering the last $hitSpan hours" >> $output
echo "\nUsage limit: 1 hit every 15 seconds & no more than 5,000 hits per day" >> $output
echo "\ntotal hits: $totalHits" >> $output
echo "users with the most hits:" >> $output
echo "                                                          user     per    per" >> $output
echo "   hits                                   remote_host    hours    hour    min" >> $output
echo "-----------------------------------------------------------------------------" >> $output

while ($checked < $size)

  # get next line 
  if ($checked == 0) then 
    set line=`head -1 xxUserCountxx`
  else
    set line=`sed "1,$checked d" xxUserCountxx | head -1`
  endif 

  # get timeSpan stats
  set num=`echo $line | gawk '{print $1}'`
  set host=`echo $line |gawk '{print $2}'`
  set timeSpan=`hgsql -N -h genome-log -e 'SELECT MAX(time_stamp) - MIN(time_stamp) \
      AS timeSpan FROM access_log WHERE remote_host = "'$host'"' apachelog`
  set timeHours=`echo $timeSpan | gawk '{printf  "%.1f", $1/3600}'`
  if ($num > 0 ) then
    set hitsPerHr=`echo $num $timeHours | gawk '{printf  "%.0f", $1/$2}'`
  else
    set hitsPerHr=`echo err  $timeHours | gawk '{printf  "%.0f", $1/$2}'`
  endif
  set hitsPerMin=`echo $hitsPerHr | gawk '{printf  "%.1f", $1/60}'`
  
  if ($debug == "true") then
    echo
    echo "host = $host"
    echo "hits = $num"
    echo "timeSpan = $timeSpan"
    echo "timeHours = $timeHours"
    echo "hitsPerHr = $hitsPerHr"
    echo "hitsPerMin = $hitsPerMin"
  endif
   

  if ($num > $max) then
    set max=$num
  endif

  # write a line in output file for this record
  # s is string, d is decimal
  echo "$num $host $timeHours $hitsPerHr $hitsPerMin " \
       | gawk '{printf("%7s %45s %8s %7s %6s\n", \
       $1, $2, $3, $4, $5)}' >> $output
  @ checked ++
end
echo >> $output

# get IPs of largest hitter and fastest hitter
set largest=`sort -nr $output | sed -e "2,$ d" | gawk '{print $2}'`
set fastest=`sort -nr +4 $output | sed -e "2,$ d" | gawk '{print $2}'`

echo "largest hitter = $largest" >> $output
echo >> $output
ipw $largest | sed -e "10,$ d" >>& $output
echo >> $output

echo "fastest hitter = $fastest" >> $output
echo >> $output
ipw $fastest | head -10 >>& $output
echo >> $output

# ----------------------------------------------
# check last hour  

hgsql -N -h genome-log -e "SELECT remote_host FROM access_log \
   WHERE time_stamp > $hourAgo" apachelog > xxLastHoursHostsxx
sort -nr xxLastHoursHostsxx | uniq -c | sort -nr | head -5 > xxHoursHitsxx 
echo "\n-------------------------------------------------------" >> $output
echo " the busiest users in the last hour: \n" >> $output
cat xxHoursHitsxx  >> $output
echo >> $output

set maxInHour=`head -1 xxHoursHitsxx | gawk '{print $1}'`
if ($maxInHour > $hourLimit) then
  set maxHourUser=`head -1 xxHoursHitsxx | gawk '{print $2}'`
  set maxHrUserPerMin=`echo $maxInHour | gawk '{printf  "%.0f", $1/60}'`
  set maxHrUserPerSec=`echo $maxHrUserPerMin | gawk '{printf  "%.1f", $1/60}'`

  echo " this user is above $hourLimit in the last hour: $maxHourUser" >> $output
  echo " $maxHrUserPerMin per min, $maxHrUserPerSec per sec\n" >> $output
  ipw $maxHourUser | head -10  >>& $output
  echo >> $output
endif

# ----------------------------------------------
# check criteria for output and print

if ($max > $threshhold || $maxInHour > $hourLimit) then
  cat $output
else
  echo "\n  no user  > $threshhold hits in last $hitSpan hours" 
  echo "  and none >  $hourLimit in last hour\n"
endif

rm remote_host
rm xxUserCountxx
rm  $output
rm xxLastHoursHostsxx
rm xxHoursHitsxx 
exit

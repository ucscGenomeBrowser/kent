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

if ($#argv < 1 || $#argv > 2) then
  # not enough command line args
  echo
  echo "  script to catch bots."
  echo "  monitors the most common IP addresses of users "
  echo "    and squawks when someone gets too high."
  echo
  echo "    usage:  threshhold (number of hits from single source), "
  echo "            [report size] (defaults to 5)"
  echo
  exit
else
  set threshhold=$argv[1]
endif

if ($#argv == 2) then
  set size=$argv[2]
endif

# get whole list from access_log
hgsql -N -h genome-centdb -e "SELECT remote_host FROM access_log" apachelog > remote_host
sort remote_host | uniq -c | sort -nr > xxUserCountxx
set totalHits=`wc -l remote_host | gawk '{print $1}'`

set checked=0
set max=0

# write header to file
rm -f $output
echo "\nfrom apachelogs.access_log" >> $output
echo "\ntotal hits: $totalHits" >> $output
echo "users with the most hits:" >> $output
echo "                                                                   per    per" >> $output
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
  set timeSpan=`hgsql -N -h genome-centdb -e 'SELECT MAX(time_stamp) - MIN(time_stamp) AS timeSpan \
      FROM access_log WHERE remote_host = "'$host'"' apachelog`
  set timeHours=`echo $timeSpan | awk '{printf  "%.1f", $1/3600}'`
  set hitsPerHr=`echo $num $timeHours | awk '{printf  "%.0f", $1/$2}'`
  set hitsPerMin=`echo $hitsPerHr | awk '{printf  "%.1f", $1/60}'`

  if ($num > $max) then
    set max=$num
  endif

  # write a line in output file for this record
  # s is string, d is decimal
  echo "$num $host $timeHours $hitsPerHr $hitsPerMin " \
       | gawk '{ printf("%7s %45s %8s %7s %6s\n", \
       $1, $2, $3, $4, $5) }' >> $output
  @ checked ++
end
echo >> $output

# get IPs of largest hitter and fastest hitter
set largest=`sort -nr $output | sed -e "2,$ d" | gawk '{print $2}'`
set fastest=`sort -nr +4 $output | sed -e "2,$ d" | gawk '{print $2}'`

echo "largest hitter = $largest" >> $output
echo >> $output
ipw $largest | sed -e "10,$ d" >> $output
echo >> $output

echo "fastest hitter = $fastest" >> $output
echo >> $output
ipw $fastest | sed -e "10,$ d" >> $output
echo >> $output

if ($max > $threshhold) then
  cat $output
else
  # stay quiet
endif

rm xxUserCountxx
rm  $output

exit

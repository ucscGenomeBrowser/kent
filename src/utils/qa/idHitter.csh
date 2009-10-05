#!/bin/tcsh
source `which qaConfig.csh`

####################
#  06-24-05 Bob Kuhn
#
#  Checks for activity of heavyHitters
#
####################

set worst=""

if ($#argv != 1) then
  echo
  echo "  checks type of activity of a heavy hitter."
  echo
  echo '      usage:  ipAddress '
  echo
  exit
else
  set hitter=$argv[1]
endif

echo "stats for $hitter"

# how many hits?
hgsql -t -h genome-log -e \
  'SELECT COUNT(*) FROM access_log WHERE remote_host = "'$hitter'"' \
  apachelog

# how long hitting?
hgsql -t -h genome-log -e 'SELECT (max(time_stamp) - min(time_stamp))/3600 \
  AS hours FROM access_log WHERE remote_host = "'$hitter'"' apachelog

# hitting what?
hgsql -t -h genome-log -e 'SELECT DISTINCT(request_uri) AS queries, \
  COUNT(*) AS number FROM access_log \
  WHERE remote_host = "'$hitter'" GROUP BY queries \
  ORDER BY number DESC LIMIT 20' apachelog

# quiet since:
set lastHitHitter=`hgsql -N -h genome-log -e 'SELECT max(time_stamp)/3600 \
  FROM access_log WHERE remote_host = "'$hitter'"' apachelog`
set lastHitAll=`hgsql -N -h genome-log -e 'SELECT max(time_stamp)/3600 \
  FROM access_log' apachelog`
set quiet=`echo $lastHitAll $lastHitHitter | awk '{printf "%.1f", $1 - $2}'`
echo
echo "\n  quiet for $quiet hours\n"

exit

set hitsPerHr=`echo $num $timeHours | gawk '{printf  "%.0f", $1/$2}'`
(pipes two values, divides them and prints as whole number)


SELECT request_uri FROM access_log WHERE remote_host = "149.142.103.54" 
AND request_uri != "/cgi-bin/hgc" AND request_uri != "/cgi-bin/hgTracks" 
AND request_uri!= "/cgi-bin/hgGene" limit 10;


SELECT COUNT(*) FROM access_log WHERE remote_host = "149.142.103.54" 
AND request_uri != "/cgi-bin/hgc" AND request_uri != "/cgi-bin/hgTracks" 
AND request_uri!= "/cgi-bin/hgGene" AND request_uri NOT LIKE "%trash%" 
limit 10;



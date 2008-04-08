#!/bin/tcsh


###############################################
# 
#  04-04-08
#  Robert Kuhn
#
#  Gets some stats on hgSession
# 
###############################################

set users=0
set count=0
set shared=0
set countTot=0
set userTot=0
set shareTot=0
set reuseTot=0

if ( $#argv != 1 ) then
  # no command line args
  echo
  echo " gets some stats on hgSession"
  echo
  echo "    usage:  go"
  echo
  exit
endif

# get months sessions has been alive:
set months=`hgsql -N -h genome-centdb -e "SELECT DISTINCT firstUse FROM namedSessionDb" hgcentral \
  | awk -F- '{print $1"-"$2}' | sort -u`

# get stats
echo
echo " month  count users  shared   reused  "
echo "------  ----- ----- -------  -------"
foreach month ( $months )
  set count=`hgsql -N -h genome-centdb -e 'SELECT COUNT(*) FROM namedSessionDb \
    WHERE firstUse like "'$month%'"' hgcentral`
  set users=`hgsql -N -h genome-centdb -e 'SELECT COUNT(DISTINCT(userName)) \
    FROM namedSessionDb WHERE firstUse like "'$month%'"' hgcentral`
  set shared=`hgsql -N -h genome-centdb -e 'SELECT COUNT(*) FROM namedSessionDb \
    WHERE firstUse like "'$month%'" AND shared = 1' hgcentral`
  set reuse=`hgsql -N -h genome-centdb -e 'SELECT firstUse, lastUse FROM namedSessionDb \
    WHERE firstUse like "'$month%'"' hgcentral \
    |  awk '$1 != $3 {print $1, $3}'  | wc -l`
  echo $month $count $users $shared $reuse  \
    | awk '{printf ("%7s %4s %5s %4s %2d%% %4s %2d%%\n", \
    $1, $2, $3, $4, $4/$2*100, $5, $5/$2*100)}'
  # do totals
  set countTot=`echo $countTot $count | awk '{print $1+$2}'`
  set userTot=`echo $userTot $users | awk '{print $1+$2}'`
  set shareTot=`echo $shareTot $shared | awk '{print $1+$2}'`
  set reuseTot=`echo $reuseTot $reuse | awk '{print $1+$2}'`
end

echo "------- ----- ----- -------  -------"
echo "total " $countTot $userTot $shareTot $reuseTot  \
  | awk '{printf ("%7s %4s %5s %4s %2d%% %4s %2d%%\n", \
  $1, $2, $3, $4, $4/$2*100, $5, $5/$2*100)}'
set uniq=`hgsql -N -h genome-centdb -e 'SELECT COUNT(DISTINCT(userName)) \
  FROM namedSessionDb' hgcentral`
echo "uniq " "-" "$uniq" \
  | awk '{printf ("%7s %4s %5s \n", $1, $2, $3)}'
echo

# see how often people make more than one session:
echo "how many people had more than one session?"
echo " people sessions"
echo " ------ --------"
hgsql -N -h genome-centdb -e 'SELECT DISTINCT(userName), COUNT(*) as number \
  FROM namedSessionDb GROUP BY userName ORDER BY number' hgcentral \
  | awk '{print $2}' | sort -n | uniq -c
echo

echo "how often are sessions reaccessed?"
hgsql -t -h genome-centdb -e 'SELECT DISTINCT(useCount), COUNT(*) as number \
  FROM namedSessionDb GROUP BY useCount ORDER BY useCount' hgcentral 
echo

echo "most used:"
hgsql -t -h genome-centdb -e 'SELECT userName, sessionName, firstUse, \
  lastUse, useCount FROM namedSessionDb ORDER BY useCount DESC LIMIT 2' hgcentral
echo

exit 0

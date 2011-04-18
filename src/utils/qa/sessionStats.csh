#!/bin/tcsh
source `which qaConfig.csh`


###############################################
# 
#  04-04-2008
#  Robert Kuhn
#
#  Gets some stats on hgSession
# 
###############################################

set short=""
set users=0
set count=0
set shared=0
set reuse=0
set countTot=0
set userTot=0
set shareTot=0
set reuseTot=0

if ( $#argv != 1 ) then
  # no command line args
  echo
  echo " gets some stats on hgSession"
  echo
  echo "    usage:  go|short"
  echo "      where short prints abbreviated report."
  echo
  exit
else
  if ( $argv[1] != "go" ) then
    if ( $argv[1] == "short" ) then
      set short=$argv[1]
    else
      echo
      echo ' only "go" are "short" are permitted as arguments'
      $0
      exit
    endif
  endif
endif

# get months sessions has been alive:
set months=`hgsql -N -h $sqlrr -e "SELECT DISTINCT firstUse FROM namedSessionDb" hgcentral \
  | awk -F- '{print $1"-"$2}' | sort -u`

# get stats
rm -f tempOutFile
echo
echo " first  count users  shared   reused  "
echo "------  ----- ----- -------  -------"
foreach month ( $months )
  set count=`hgsql -N -h $sqlrr -e 'SELECT COUNT(*) FROM namedSessionDb \
    WHERE firstUse LIKE "'$month%'"' hgcentral`
  set users=`hgsql -N -h $sqlrr -e 'SELECT COUNT(DISTINCT(userName)) \
    FROM namedSessionDb WHERE firstUse like "'$month%'"' hgcentral`
  set shared=`hgsql -N -h $sqlrr -e 'SELECT COUNT(*) FROM namedSessionDb \
    WHERE firstUse LIKE "'$month%'" AND shared = 1' hgcentral`
  set reuse=`hgsql -N -h $sqlrr -e 'SELECT firstUse, lastUse FROM namedSessionDb \
    WHERE firstUse LIKE "'$month%'"' hgcentral \
    |  awk '$1 != $3 {print $1, $3}'  | wc -l`
  echo $month $count $users $shared $reuse  \
    | awk '{printf ("%7s %4s %5s %4s %2d%% %4s %2d%%\n", \
    $1, $2, $3, $4, $4/$2*100, $5, $5/$2*100)}' | tee -a tempOutFile

  # increment totals
  set countTot=`echo $countTot $count | awk '{print $1+$2}'`
  set userTot=`echo $userTot $users | awk '{print $1+$2}'`
  set shareTot=`echo $shareTot $shared | awk '{print $1+$2}'`
  set reuseTot=`echo $reuseTot $reuse | awk '{print $1+$2}'`
end

echo "------- ----- ----- -------  -------"
echo "total " $countTot $userTot $shareTot $reuseTot  \
  | awk '{printf ("%7s %4s %5s %4s %2d%% %4s %2d%%\n", \
  $1, $2, $3, $4, $4/$2*100, $5, $5/$2*100)}'
set uniq=`hgsql -N -h $sqlrr -e 'SELECT COUNT(DISTINCT(userName)) \
  FROM namedSessionDb' hgcentral`
echo "uniq " "-" "$uniq" \
  | awk '{printf ("%7s %4s %5s \n", $1, $2, $3)}'

echo "------  ----- ----- -------  -------"
echo " first  count users  shared   reused  "
echo

# graph sessions
echo sessions
echo --------
graph.csh tempOutFile

# graph users
echo " users"
echo --------
rm -f tempOutFile2
cat tempOutFile | awk '{print $1, $3}' > tempOutFile2
graph.csh tempOutFile2
rm -f tempOutFile
rm -f tempOutFile2

# stop here if short
if ( $short != "" ) then
  exit
endif 

echo
# see how often people make more than one session:
echo "how many people had more than one session?"
echo " people sessions"
echo " ------ --------"
hgsql -N -h $sqlrr -e 'SELECT DISTINCT(userName), COUNT(*) as number \
  FROM namedSessionDb GROUP BY userName ORDER BY number' hgcentral \
  | awk '{print $2}' | sort -n | uniq -c
echo

echo "how often are sessions reaccessed?"
hgsql -t -h $sqlrr -e 'SELECT DISTINCT(useCount), COUNT(*) as sessions \
  FROM namedSessionDb GROUP BY useCount ORDER BY useCount' hgcentral 
echo

echo "most used:"
hgsql -t -h $sqlrr -e 'SELECT userName, sessionName, firstUse, \
  lastUse, useCount FROM namedSessionDb ORDER BY useCount DESC LIMIT 4' hgcentral
echo

exit 0

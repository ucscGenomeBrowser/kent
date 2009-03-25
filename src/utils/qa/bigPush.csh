#!/bin/tcsh


################################
#  04-02-04
#  updated:
#  04-08-04
#
#  Runs through set of all tables ever used in this assembly.
#  Pushes multiple tables from dev to beta
#  can't use "&" after output command because of "password prompt"
#  (if you do, each command gets put into background and 
#      requires "fg" to get to password prompt)
#  can't redirect output into file: 
#      use "script filename.out" to capture ?
#  also records total size of the push
#
################################

set db=""
set tablelist=""

if ($2 == "") then
  echo 
  echo " pushes tables in list to beta and records size."
  echo " do not redirect output ! "
  echo " do not run in the background:"
  echo " (will hang on long tables due to pasword prompt)."
  echo " reports total size of push."
  echo
  echo "   usage:  database, tablelist"
  echo
  exit
else
  set db=$1
  set tablelist=$2
endif

set trackName=`echo $2 | sed -e "s/Tables//"`
# echo trackName = $trackName

echo
echo "will have to re-type Password after long tables (timeout)"
echo
rm -f $db.$trackName.push
foreach table (`cat $tablelist`)
  echo pushing "$table"
  sudo mypush $db "$table" hgwbeta >> $db.$trackName.push
  echo "$table" >> $db.$trackName.push
  # tail -f $db.$trackName.push
end
echo


# --------------------------------------------
# "check that all tables were pushed:"

echo
~/bin/updateTimes.csh $db $tablelist
echo


# --------------------------------------------
# "find the sizes of the pushes:"

echo
echo "find the sizes of the pushes:"
echo
grep 'total size' $db.$trackName.push | gawk '{total+=$4} END {print total}' \
   > $db.$trackName.pushSize
set size=`cat $db.$trackName.pushSize`
echo "$size\n    bytes"
echo
echo $size | gawk '{print $1/1000;print "    kilobytes"}'
echo
echo $size | gawk '{print $1/1000000;print "    megabytes"}'
echo
echo

echo end.

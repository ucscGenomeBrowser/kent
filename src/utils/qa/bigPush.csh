#!/bin/tcsh
source `which qaConfig.csh`


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

set warningMessage="\nPushes tables in list to mysqlbeta and records size.\n\
Requires sudo access to mypush to run.\n\
\n\
Do not redirect output or run in the background,\n\
as it will require you to type your password in.\n\
Program will ask you for your password again after\n\
large tables. If you take too long to re-type in\n\
the table the script stalled on might not get\n\
pushed. Double-check that all tables have been\n\
pushed!\n\
\n\
Will report total size of push and write two files:\n\
db.tables.push -> output for all tables from mypush\n\
db.tables.pushSize -> size of push\n"


if ($2 == "") then
  echo $warningMessage
  exit
else
  set db=$1
  set tablelist=$2
endif

set trackName=`echo $2 | sed -e "s/Tables//"`
# echo trackName = $trackName

echo
echo "Will have to re-type password after large tables"
echo "If you take too long to re-type your password, the table"
echo "the script stalled on might not get pushed."
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
updateTimes.csh $db $tablelist
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

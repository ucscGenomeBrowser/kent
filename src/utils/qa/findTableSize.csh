#!/bin/tcsh

################################
#  updated:
#  04-21-04, 09-01-04
#
#  Gets the size of each table in the list and totals them.
#  Only has access to dev.
#
################################

set db=""
set tablelist=""

if ($#argv < 1 || $#argv > 2 ) then
  # no command line args
  echo
  echo "  gets the size of each table in the list and totals them."
  echo "  will take single tablename."
  echo "  only has access to dev."
  echo
  echo "    usage: database, [tablelist] (defaults to whole db)"
  echo
  exit
else
  set db=$1
endif

echo
if ($#argv == 2) then
  # it's either a list of names or a single tablename
  set tablelist=$argv[2]
  if (! -e $tablelist) then
    # must be a single table.  make a list with only the tablename in it
    echo $argv[2] > tablelist
    set tablelist="tablelist"
  endif
else
  # get entire list from database
  hgsql -N -e "SHOW TABLES" $db | grep -v "trackDb" | grep -v "hgFindSpec" > tablelist
  echo "trackDb hgFindSpec" >> tablelist
  echo 'fetching tablelist.  ignoring "_username" tables'
  set tablelist="tablelist"
endif

echo
echo "using: db= "$db
echo "tablelist= "$tablelist



# --------------------------------------------
#  process sizes

rm -f tablesize
rm -f totalsize
foreach table (`cat $tablelist`)
  sudo tablesize $db $table >> tablesize
end

echo
set size=`awk '{total+=$1} END {print total}' tablesize`
echo "$size\n    kilobytes"
echo
echo $size | gawk '{printf "%.2f\n", $1/1000;print "    megabytes"}'
echo

rm -f tablesize
rm -f tablelist

#!/bin/tcsh

################################
#  05-19-04
#  gets the rowcount for a list of tables.
#
################################

set db=""
set tablelist=""
set argFlag=0
set all=""
set outname=""

if ($#argv < 2) then
  echo
  echo "  gets the rowcount for a list of tables from dev and beta."
  echo
  echo "    usage:  database, tablelist (can be just name of single table),"
  echo "            [all] (defaults to reporting about only tables that differ)."
  echo
  exit
else
  set db=$argv[1]
  set tablelist=$argv[2]     # file of tablenames or single table name
  if ($#argv > 2) then
    set all=$argv[3]
  endif
endif

echo
# echo "database: $db"


echo
if (-e $tablelist) then
  echo "running countRows for tables:"
  cat $tablelist
  echo
endif

# if $argv[2] was a track name, not a list of tables, make a file
if (! -e $tablelist) then
  echo $argv[2] > xxtablelistxx
  set tablelist="xxtablelistxx"
  set outname=$argv[2]
else
  # strip dbname from tablelist name, if present
  set outname=`echo $tablelist | sed -e "s/$db\.//"`
endif

# echo "outname = $outname"

rm -f $db.$outname.rowcounts
rm -f $db.$outname.badcounts
foreach table (`cat $tablelist`)
  set dev=`hgsql -N -e "SELECT COUNT(*) FROM $table" $db`
  set beta=`hgsql -h hgwbeta -N -e "SELECT COUNT(*) FROM $table" $db`
  if ($dev != $beta) then
    echo $table >> $db.$outname.badcounts
    echo $table >> $db.$outname.rowcounts
    echo "=============" >> $db.$outname.rowcounts
    echo "."$dev >> $db.$outname.rowcounts
    echo "."$beta >> $db.$outname.rowcounts
    echo >> $db.$outname.rowcounts
  else
    if ($all == "all") then
      # save info on equal counts, too
      echo $table >> $db.$outname.rowcounts
      echo "=============" >> $db.$outname.rowcounts
      echo "."$dev >> $db.$outname.rowcounts
      echo "."$beta >> $db.$outname.rowcounts
      echo >> $db.$outname.rowcounts
    endif
  endif
end

# clean out file
rm -f xxtablelistxx

# show results
if (-e $db.$outname.rowcounts) then
  # echo "dev first"
  echo
  cat $db.$outname.rowcounts
else
  echo "all tables have same count of rows"
  echo
endif

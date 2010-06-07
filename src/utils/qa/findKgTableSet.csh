#!/bin/tcsh

################################
#  03-25-04
#  updated to take command line only
#
#  Runs through set of all tables ever used in KG, FB and PB.
#  Expects kgTables, fbTables and pbTables up one directory level
#  Checks dev for which of those tables are used in this assembly.
#  Makes files called kgTables$db, fbTables$db and pbTables$db 
#      for each of the input tablelists
#  Checks update times on dev and beta for table in *Tables$db
#      to see if table has been updated.
# 
################################

set db=""
set tablePath="/cluster/bin/scripts"
set curr_dir=$cwd


if ($1 == "") then
  # no command line args
  echo
  echo "  gets set of relevant tables for KG, FB, PB from master list."
  echo "  expects lists kgTables, fbTables and pbTables up one directory."
  echo "  makes files xxTables$db for current assembly."
  echo "  does not detect if list is incomplete."
  echo "  compares update times dev to beta."
  echo
  echo "    usage:  database"
  echo
  exit
else
  set db=$argv[1]
endif

cp $tablePath/kgTables .
cp $tablePath/gsTables .
cp $tablePath/pbTables .

# --------------------------------------------
# run through table list for KG and FB and pull out tables that have
#  tables on dev for this assembly:

echo
foreach tableList  ( kgTables gsTables pbTables )
  if ( ! -e $tableList ) then
    echo "$tablelist needed locally"
    exit 1
  endif
  hgsql -N -e 'SHOW TABLES' $db | grep -w -f $tableList > $db.$tableList 
end

foreach tableList  ( kgTables gsTables pbTables )
  echo
  echo "$tableList tables not in ${db}:"
  echo
  cat $tableList | grep -w -v -f $db.$tableList 
end
echo

exit


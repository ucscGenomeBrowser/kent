#!/bin/tcsh

###############################################
# 
#  07-14-04
#  Will get a column from a table on dev and beta and check diffs
# 
###############################################

set db=""
set table=""
set column=""


if ($#argv != 3) then
  # no command line args
  echo
  echo "  gets a column from a table on dev and beta and checks diffs."
  echo "  reports numbers of rows unique to each and common"
  echo "  writes files of everything."
  echo
  echo "    usage:  database, table, column "
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
  set column=$argv[3]
endif

# --------------------------------------------
#  get sets of KG, FB, PB tables:

hgsql -N -e "SELECT $column FROM $table" $db | sort  > $db.$table.$column.dev
hgsql -N -h hgwbeta -e "SELECT $column FROM $table" $db | sort  > $db.$table.$column.beta
comm -23 $db.$table.$column.dev $db.$table.$column.beta >$db.$table.$column.devOnly
comm -13 $db.$table.$column.dev $db.$table.$column.beta >$db.$table.$column.betaOnly
comm -12 $db.$table.$column.dev $db.$table.$column.beta >$db.$table.$column.common
wc -l $db.$table.$column.devOnly $db.$table.$column.betaOnly $db.$table.$column.common


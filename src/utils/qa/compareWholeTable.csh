#!/bin/tcsh

###############################################
# 
#  07-14-04
#  Robert Kuhn
# 
#  Gets an entire table from dev and beta and check diffs
# 
###############################################

set db=""
set table=""
set column=""


if ($#argv != 2) then
  # no command line args
  echo
  echo "  gets an entire table from dev and beta and checks diffs."
  echo "  reports numbers of rows unique to each and common."
  echo "  writes files of everything."
  echo
  echo "    usage:  database, table"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

# --------------------------------------------

hgsql -N -e "SELECT * FROM $table" $db | sort  > $db.$table.dev
hgsql -N -h hgwbeta -e "SELECT * FROM $table" $db | sort  > $db.$table.beta
comm -23 $db.$table.dev $db.$table.beta >$db.$table.devOnly
comm -13 $db.$table.dev $db.$table.beta >$db.$table.betaOnly
comm -12 $db.$table.dev $db.$table.beta >$db.$table.common
echo
wc -l $db.$table.devOnly $db.$table.betaOnly $db.$table.common | grep -v "total"
echo


#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  07-14-04
#  Robert Kuhn
#
#  Gets a column from a table on dev and beta and checks diffs
# 
###############################################

set db=""
set db2=""
set table=""
set column=""


if ($#argv < 3 || $#argv > 4) then
  # no command line args
  echo
  echo "  gets a column from a table on dev and beta and checks diffs."
  echo "  reports numbers of rows unique to each and common."
  echo "  can compare to older database."
  echo "  writes files of everything."
  echo
  echo "    usage:  database table column [db2] "
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
  set column=$argv[3]
  set db2=$db
endif

if ($#argv == 4) then
  set db2=$argv[4]
endif 

# --------------------------------------------
#  get files of tables and compare:

hgsql -N -e "SELECT $column FROM $table" $db | sort  > $db.$table.$column.dev
hgsql -N -h $sqlbeta -e "SELECT $column FROM $table" $db2 | sort  > $db2.$table.$column.beta
comm -23 $db.$table.$column.dev $db2.$table.$column.beta >$db.$table.$column.devOnly
comm -13 $db.$table.$column.dev $db2.$table.$column.beta >$db2.$table.$column.betaOnly
comm -12 $db.$table.$column.dev $db2.$table.$column.beta >$db.$table.$column.common
echo
wc -l $db.$table.$column.devOnly $db2.$table.$column.betaOnly \
  $db.$table.$column.common | grep -v "total"
echo


#!/bin/tcsh
source `which qaConfig.csh`

################################
#  05-19-04
#  gets the rowcount for a list of tables.
#
################################

set db=""
set tablelist=""
set tables=""

if ($#argv != 2) then
  echo
  echo "  gets the rowcount for a list of tables from dev and beta."
  echo
  echo "    usage:  database tablelist"
  echo "      tablelist can be just name of single table"
  echo
  exit
else
  set db=$argv[1]
  set tablelist=$argv[2]     # file of tablenames or single table name
endif

echo "database $db"
echo "tablelist $tablelist"

echo
if ( -e $tablelist ) then
  echo "running countRows for tables:"
  set tables=`cat $tablelist`
  echo "tables $tables"
  echo
else
  set tables=$tablelist
endif

foreach table ( $tables )
  set dev=`hgsql -N -e "SELECT COUNT(*) FROM $table" $db` 
  set beta=`hgsql -h $sqlbeta -N -e "SELECT COUNT(*) FROM $table" $db`
  echo $table
  echo "============="
  echo "."$dev 
  echo "."$beta 
  echo
end


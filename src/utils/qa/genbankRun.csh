#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  11-08-2005
#  Ann Zweig
#
#  checks to see if genbank is running on the RR for a particular assembly
#
################################

set db=""
set count=0
set newCount=0
set machine="hgw1"
set tableName="gbCdnaInfo"

if ( $#argv != 1 ) then
  # no arguments
  echo
  echo " checks to see if genbank is running on the RR for a given assembly." 
  echo " (should take less than one minute to run.)"
  echo
  echo "    usage: genbankRun.csh database_name"
  echo
  exit
else
  set db=$argv[1]
endif


# check to see if the database exists
set count = `getRRdatabases.csh $machine | grep -c -i $db`

if ($count != 1) then
  echo "ERROR: the $db database does not exist on $machine.  Exiting."
  exit
endif
echo

# check to see if the existing database has a $tableName table
set newCount = `getAssemblies.csh $tableName $machine | grep -c -i $db`

# if the $tableName is present in this $db on this $machine,
# there will be 2 instances of $db in the output
if ($newCount != 2) then
  echo "ERROR: the $db database does not have a $tableName table on $machine.  Exiting."
  exit
endif


# database is present, go ahead with the updateTimes script
echo "This shows the last time the genbank $tableName table was updated"
echo "for $db on hgwbeta (listed first) and on hgw1 (listed second):"

updateTimes.csh $db $tableName | grep '.' | tail -2



#!/bin/tcsh
source `which qaConfig.csh`

#######################
#
#  01-17-06
#  determines if table is in split format and returns split name if so
#  pulled out of getAssebmlies.csh
#  Robert Kuhn
#
#######################

set tablename=""
set machine="hgwdev"
set host=""
set db=""

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  determines if table is in split format "
  echo "    and returns split name if so."
  echo
  echo "    usage:  db tablename [hgwdev | hgwbeta]"
  echo
  echo "      third argument accepts machine, defaults to hgwdev"
  echo
  exit
else
  set db=$argv[1]
  set tablename=$argv[2]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script from dev!\n"
 exit 1
endif

# assign command line arguments
if ( $#argv == 3 ) then
  if ( $argv[3] == "hgwbeta" ) then
    set machine="$argv[3]"
    set host="-h $sqlbeta"
  else
    if ( $argv[3] != "hgwdev" ) then
      echo "only hgwdev and hgwbeta are allowed in $0."
      exit 1
    endif
  endif
endif

# echo "db = $db"
# echo "tablename = $tablename"
# echo "machine = $machine"
# echo "host = $host"

# -------------------------------------------------
# get all assemblies containing $tablename

set chrom=""

# check for chromInfo table
hgsql -N $host -e 'SHOW TABLES' $db | grep -qw "chromInfo" 
if ( ! $status ) then
  set chrom=`hgsql -N $host -e 'SELECT chrom FROM chromInfo LIMIT 1' $db`
  # check if split table
  hgsql -N $host -e 'SHOW TABLES' $db | grep -qw $tablename 
  if ( ! $status ) then
    echo "unsplit"
  else
    hgsql -N $host -e 'SHOW TABLES' $db | grep -qw ${chrom}_$tablename 
    if ( ! $status ) then
      echo "$chrom"
    else
      echo
      echo " no such table $tablename or ${chrom}_$tablename."
      echo
      exit 1
    endif
  endif
  exit 0
else
  echo
  echo "no chromInfo table for $db on ${machine}."
  echo
  exit 1
endif


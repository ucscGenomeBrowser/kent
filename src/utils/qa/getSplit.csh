#!/bin/tcsh

#######################
#
#  01-17-06
#  determines if table is in split format and returns split name if so
#  split out of getAssebmlies.csh
#  Robert Kuhn
#
#######################

set tablename=""
set machine="hgwbeta"
set host="-h hgwbeta"
set db=""

if ($#argv < 2 || $#argv > 3) then
  echo
  echo "  determines if table is in split format "
  echo "    and returns split name if so."
  echo
  echo "    usage:  db, tablename, [machine: hgwdev|beta] defaults to beta"
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
if ($#argv == 3) then
  set machine="$argv[3]"
  set host="-h $argv[3]"
  if ($argv[3] == "hgwdev") then
    set host=""
  endif
endif

# echo "db = $db"
# echo "tablename = $tablename"
# echo "machine = $machine"
# echo "host = $host"

# check machine validity

checkMachineName.csh $machine
if ( $status ) then
  echo "${0}:"
  $0
  exit 1
endif

# -------------------------------------------------
# get all assemblies containing $tablename

set chrom=""
set split=""
set isChromInfo=0
if ( $machine == hgwdev || $machine == hgwbeta ) then
  # check for chromInfo table
  set isChromInfo=`hgsql -N $host -e 'SHOW TABLES' $db | grep "chromInfo" \
     | wc -l`
  if ( $isChromInfo > 0 ) then
    set chrom=`hgsql -N $host -e 'SELECT chrom FROM chromInfo LIMIT 1' $db`

    # check if split table
    set split=`hgsql -N $host -e 'SHOW TABLES LIKE "'${chrom}_$tablename'"' \
      $db | wc -l`
    if ( $split == 1 ) then
      echo "split $chrom"
    else 
      echo "unsplit"
    endif
    exit 0
  else
    echo "no chromInfo table.  split irrelevant"
    exit 1
  endif
else   # not dev or beta
  echo "\n  only works on dev and beta\n"
  echo "${0}:"
  $0
echo


#!/bin/tcsh
source `which qaConfig.csh`

###############################################
#  09-20-07
# 
#  Finds the proper column names if "chrom", "tName" or "genoName".'
# 
###############################################

set db=""
set table=""
set chr=""

if ( $#argv != 2 ) then
  echo
  echo '  Finds the proper column names if "chrom", "tName" or "genoName".'
  echo
  echo "    usage:  database table"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

set split=`getSplit.csh $db $table`
if ( $status ) then
  echo "problem in getSplit.csh"
  exit 1
endif
if ( $split != "unsplit" ) then
  set table="${split}_$table"
endif

set chr=`hgsql -N -e "DESC $table" $db | gawk '{print $1}' | egrep -w "chrom|tName|genoName"`
if ( $status ) then
  echo '\n  '$db.$table' has no "chrom", "tName" or "genoName" fields.\n'
  echo ${0}:
  $0
  exit 1 
endif 
echo $chr
exit

#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  12-13-05
#  Robert Kuhn
#
#  check to see if there are genes on all chroms.
# 
###############################################


if ( "$HOST" != "hgwdev" ) then
 echo "\n  error: you must run this script on dev!\n"
 exit 1
endif

set db=""
set oldDb=""
set table=""
set host2=""
set chrom=""
set chroms=""
set old=""
set new=""
set machineOut=""
set split=""
set regular=""
set random=""

if ( $#argv < 2 ||  $#argv > 4 ) then
  # no command line args
  echo
  echo "  check to see if there are annotations on all chroms."
  echo "  will check to see if chrom field is named tName or genoName."
  echo
  echo "    usage:  database1 table [database2] [RR]"
  echo
  echo "      checks database1 on dev"
  echo "      database2 will be checked on beta by default"
  echo "        if RR is specified, will use genome-mysql"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

if ( $#argv == 3 ) then
  if ( $argv[3] == "RR" ) then
    set host2="mysql -h genome-mysql -u genome -A"
    set oldDb=$db
    set machineOut="(${argv[3]})"
  else
    set host2="hgsql -h $sqlbeta"
    set machineOut="(hgwbeta)"
    if ( $argv[3] == "hgwbeta" ) then
      # allow use of "hgwbeta" to check same db in two places
      set oldDb=$db
    else
      # argv[3] must be a db
      set oldDb=$argv[3] 
    endif
  endif
endif

if ( $#argv == 4 ) then
  set oldDb=$argv[3]
  set machineOut="(${argv[4]})"
  if ( $argv[4] == "hgwbeta" ) then
    set host2="hgsql -h $sqlbeta"
  else 
    if ( $argv[4] == "RR" ) then
      set host2="mysql -h genome-mysql -u genome -A"
    else
      echo
      echo "4th parameter must be RR or hgwbeta"
      echo
      $0
      exit 1
    endif
  endif
endif

# echo "db = $db"
# echo "oldDb = $oldDb"
# echo "machineOut = $machineOut"
# echo "table = $table"
# echo "host2 = $host2"

set chroms=`hgsql -N -e "SELECT chrom FROM chromInfo" $db`
set split=`getSplit.csh $db $table`
if ( $status ) then
  echo "\n  the database or table may not exist\n"
  exit
endif

if ( $split == "unsplit" ) then
  set split=""
else
  set split=${split}_
  echo "\n  split tables. e.g., $split$table"
endif

set chrom=`getChromFieldName.csh $db $split$table`
if ( $status ) then
  echo "  error getting chromFieldName."
  echo "   chrom, genoName or tName required."
  echo
  exit 1
endif 

echo

# output header
echo "chrom \t$db \t$oldDb$machineOut"

# do randoms last
set regular=`echo $chroms | sed -e "s/ /\n/g" | grep -v random`
set  random=`echo $chroms | sed -e "s/ /\n/g" | grep random`

foreach c ( $regular $random )
  if ( $split != "" ) then
    set table="${c}_$table"
  endif
  set new=`nice hgsql -N -e 'SELECT COUNT(*) FROM '$table' \
     WHERE '$chrom' = "'$c'"' $db`
  if ( $machineOut != "" ) then
    set old=`$host2 -Ne 'SELECT COUNT(*) FROM '$table' \
      WHERE '$chrom' = "'$c'"' $oldDb`
    set old=`nice $host2 -Ne 'SELECT COUNT(*) FROM '$table' \
      WHERE '$chrom' = "'$c'"' $oldDb`
  endif 

  # output
  echo "$c\t$new\t$old"
  set table=$argv[2]
end


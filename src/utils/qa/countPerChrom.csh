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
set tableName=""
set host2=""
set chrom="chrom"
set chroms=""
set old=""
set new=""
set machineOut=""
set split=""

if ( $#argv < 2 ||  $#argv > 4 ) then
  # no command line args
  echo
  echo "  check to see if there are annotations on all chroms."
  echo "    will check to see if chrom field is named tName or genoName."
  echo
  echo "    usage:  database table [oldDb] [other machine]"
  echo
  echo "      where oldDb must be specified if other machine is used"
  echo "      oldDb will be checked on other machine"
  echo "      defaults to hgwdev"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
  set tableName=$table
endif

if ( $#argv > 2 ) then
  set oldDb=$argv[3]
endif

if ( $#argv == 4 ) then
  set machineOut="(${argv[4]})"
  set host2="-h $argv[4]"
endif

# echo "db = $db"
# echo "oldDb = $oldDb"
# echo "machineOut = $machineOut"
# echo "table = $table"

set chroms=`hgsql -N -e "SELECT chrom FROM chromInfo" $db`
set split=`getSplit.csh $db $table`
if ( $status ) then
  echo "\n  maybe the database is not present?\n"
  exit
endif

if ( $split != "unsplit" ) then
  set tableName=${split}_$table
  echo "\n  split tables. e.g., $tableName"
endif

set chrom=`getChromFieldName.csh $db $tableName`
if ( $status ) then
  echo "  error getting chromFieldName."
  echo "   chrom, genoName or tName required."
  echo
  exit 1
endif 

echo

# output header
echo "chrom \t$db \t$oldDb$machineOut"

foreach x ( `echo $chroms | sed -e "s/ /\n/g" | grep -v random` )
  if ( $split != "unsplit" ) then
    set table="${x}_$table"
  endif
  set new=`nice hgsql -N -e 'SELECT COUNT(*) FROM '$table' \
     WHERE '$chrom' = "'$x'"' $db`
  if ( $machineOut != "" ) then
    set old=`nice hgsql -N $host2 -e 'SELECT COUNT(*) FROM '$table' \
      WHERE '$chrom' = "'$x'"' $oldDb`
  endif 
  # output
  echo "$x\t$new\t$old"
  set table=$argv[2]
end

# do randoms separately
foreach x (`echo $chroms | sed -e "s/ /\n/g" | grep random`)
  if ($split != "unsplit") then
    set table="${x}_$table"
  endif
  set new=`nice hgsql -N -e 'SELECT COUNT(*) FROM '$table' \
     WHERE '$chrom' = "'$x'"' $db`
  if ( $machineOut != "" ) then
    set old=`nice hgsql -N $host2 -e 'SELECT COUNT(*) FROM '$table' \
      WHERE '$chrom' = "'$x'"' $oldDb`
  endif
  echo "$x\t$new\t$old"
  set table=$argv[2]
end


#!/bin/tcsh

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
set machine2=""
set host2=""
set chrom="chrom"
set chroms=""
set old=""
set new=""
set onlyOne=0
set machineOut=""
set ok=0

if ($#argv < 2 ||  $#argv > 4) then
  # no command line args
  echo
  echo "  check to see if there are genes on all chroms."
  echo "    will check to see if chrom field is named tName or genoName."
  echo
  echo "    usage:  database, table, [oldDb], [other machine]"
  echo "      where oldDb must be specified if other machine is used"
  echo "      oldDb will be checked on other machine"
  echo "      defaults to hgwdev"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

if ($#argv > 2) then
  set oldDb=$argv[3]
else
  set onlyOne=1
endif

if ($#argv == 4) then
  set oldDb=$argv[3]
  set machine2=$argv[4]
  set machineOut="(${argv[4]})"
  set host2="-h $argv[4]"
endif

# echo "db = $db"
# echo "oldDb = $oldDb"
# echo "machine2 = $machine2"
# echo "table = $table"

# check if maybe chrom is called tName
hgsql -e "DESC $table" $db | egrep -w tName > /dev/null 
if (! $status ) then
  set chrom="tName"
  echo 'chrom is "tName"'
  set ok=1
endif 

# check if maybe chrom is called genoName (prob only rmsk)
hgsql -e "DESC $table" $db | egrep -w genoName > /dev/null 
if (! $status ) then
  set chrom="genoName"
  echo 'chrom is "genoName"'
  set ok=1
endif 

# final check to see chrom-like field present
hgsql -e "DESC $table" $db | egrep -w chrom > /dev/null 
if (! $status ) then
  set ok=1
endif 

if ($ok == 0) then
  echo
  echo " no appropriate field for this table."
  echo "   chrom, genoName or tName required."
  echo
  exit
endif 

echo

set chroms=`hgsql -N -e "SELECT chrom FROM chromInfo" $db`

# output header
echo "chrom \t$db \t$oldDb$machineOut"

foreach x (`echo $chroms | sed -e "s/ /\n/g" | grep -v random`)
  set new=`nice hgsql -N -e 'SELECT COUNT(*) FROM '$table' \
     WHERE '$chrom' = "'$x'"' $db`
  if ($onlyOne == 0) then
    set old=`nice hgsql -N $host2 -e 'SELECT COUNT(*) FROM '$table' \
      WHERE '$chrom' = "'$x'"' $oldDb`
  endif 
  # output
  echo "$x\t$new\t$old"
end

# do randoms separately
foreach x (`echo $chroms | sed -e "s/ /\n/g" | grep random`)
  set new=`nice hgsql -N -e 'SELECT COUNT(*) FROM '$table' \
     WHERE '$chrom' = "'$x'"' $db`
  if ($onlyOne == 0) then
    set old=`nice hgsql -N $host2 -e 'SELECT COUNT(*) FROM '$table' \
      WHERE '$chrom' = "'$x'"' $oldDb`
  endif
  echo "$x\t$new\t$old"
end


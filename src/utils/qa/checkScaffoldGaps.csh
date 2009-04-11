#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  12-01-04
#  Robert Kuhn
# 
#  Checks that gaps and gold add up to size for scaffold-based assemblies.
# 
###############################################

set db=""
set table=""
set column=""


if ($#argv != 1) then
  # no command line args
  echo
  echo "  checks that gaps and gold add up to size for scaffold-based assemblies."
  echo
  echo "    usage:  database"
  echo
  exit
else
  set db=$argv[1]
endif

# --------------------------------------------

set count=0
set errFlag="0"
hgsql -N -e "SELECT chrom FROM chromInfo" $db | sort  > $db.chromInfo.dev
foreach chrom (`cat $db.chromInfo.dev`)
  set count=`echo $count | gawk '{print $1 + 1}'`
  set size=`hgsql -N -e 'SELECT size FROM chromInfo WHERE chrom = "'$chrom'"' $db`
  set gaps=`hgsql -N -e 'SELECT SUM(chromEnd) - SUM(chromStart) FROM gap WHERE chrom = "'$chrom'"' $db`
  set gold=`hgsql -N -e 'SELECT SUM(chromEnd) - SUM(chromStart) FROM gold \
       WHERE chrom = "'$chrom'"' $db`
  set totalsize=`echo "$gaps $gold" | gawk '{print $1 + $2}'`
  if ($totalsize != $size) then
    echo "gap + gold not equal to size for $chrom"
    echo "gap + gold not equal to size for $chrom" >> $db.gapGoldWrong
    set errFlag="1"
  else
    echo $count $chrom
  endif
end

if ($errFlag == "0") then
  echo "gap + gold = chromInfo.size for all chroms."
else
  echo cat $db.gapGoldWrong
endif
# rm -f $db.chromInfo.dev

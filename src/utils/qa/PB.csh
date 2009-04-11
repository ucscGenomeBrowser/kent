#! /bin/tcsh
source `which qaConfig.csh`


############################
#  02-15-04
#  04-14-04
#  updated again.  added $db support, command line
#  sets swissprot database by query from gdbPdb for hgwtest
#
#  Runs Proteome Browser database testing
#
############################

set db=""

if ($1 == "") then
  echo
  echo "  runs Proteome Browser testing."
  echo "  if not given on command line, list of tables, pbTablesAll, \
             is expected up one directory."
  echo
  echo "    usage: database, [tablelist]"
  echo
  exit
else
  set db=$1
endif

sort ../pbTablesAll > pbTablesAll
set tablelist="pbTablesAll"

if ($2 != "") then
  set tablelist=$2
endif

echo "using:"
echo "db=$db"

echo "tablelist=$tablelist"
echo


# --------------------------------------------
# set proteins database by query from gdbPdb for hgwtest:


set protDb=`hgsql -N -e 'SELECT proteomeDb FROM gdbPdb \
    WHERE genomeDb = "'$db'"' hgcentraltest `
echo protDb = $protDb
echo

# --------------------------------------------
# checking off-end and tStart - tEnd relationship

# echo "transcription coords"
# echo "printing tx to file"
# print tx to file:
hgsql -e "SELECT chromInfo.chrom, chromInfo.size - MAX(kgProtMap2.tEnd) \
   AS dist_from_end FROM chromInfo, kgProtMap2 \
   WHERE chromInfo.chrom = kgProtMap2.tName \
   GROUP BY chromInfo.chrom" $db > $db.PB.tx.offEnd
# echo


echo
echo "lines from $db.PB.tx.offEnd > 0:"
awk '{if($2<0) {print $2} }' $db.PB.tx.offEnd
echo "expect blank or check file $db.PB.tx.offEnd"
echo


# --------------------------------------------
# checking pbAnomLimit for low anomaly value less than high

echo
echo "checking pbAnomLimit for low anomaly value less than high"
hgsql -e "SELECT * FROM pbAnomLimit WHERE pctLow >= pctHi" $db
echo "expect blank"
echo

echo
echo "checking pbAnomLimit that low anomaly value is positive or zero"
echo "prints if zero or negative"
hgsql -e "SELECT * FROM pbAnomLimit WHERE pctLow <= 0" $db
echo

# --------------------------------------------
# checking pbResAvgStd -- check that all avg add to 1.00

echo
echo "checking pbResAvgStd -- check that all avg add to 1.00" 
hgsql -e "SELECT avg FROM pbResAvgStd" $db \
   | gawk '{total+=$1} END {print total}'
echo "pbResAvgStd  --  value should be one"
echo


# --------------------------------------------
# pbStamp  -- check that all columns have entries and {x,y}{max,min} make sense

echo
echo "pbStamp  -- check that all columns have entries and {x,y}{max,min} make sense"
hgsql -t -e "SELECT * FROM pbStamp" $db
echo


# --------------------------------------------
# pbAaDist* -- check that all columns have data and check the SUM(y) of each

echo
echo "pbAaDist* -- check that all columns have data and check the SUM(y) of each"
echo
rm -f AADist.totals
cat $tablelist | grep pbAaDist > AAlist1
foreach table (`cat AAlist1`)
  set cnt=`hgsql -N -e "SELECT SUM(y) FROM $table" $db`
  echo "$table\t$cnt" >> AADist.totals
end
cat AADist.totals
echo
echo "AADist* -- all values should be close to the same"
echo


# --------------------------------------------
# checking contents and sizes of non-AA Dist tables

echo
echo "checking contents and sizes of non-AA Dist tables"
echo
rm -f stamp.totals
echo "table\tsum(y)" >> stamp.totals
echo "-----\t------" >> stamp.totals
cat $tablelist | grep pep | grep Dist > pepList
foreach table (`cat pepList`)
  # hgsql -e "SELECT * FROM $table limit 1" $db
  echo
  set sum=`hgsql -N -e "SELECT SUM(y) from $table" $db`
  echo "$table\t$sum" >> stamp.totals
end
cat stamp.totals
echo
echo "pepResDist should equal 1.000"
echo

# --------------------------------------------

echo
echo "pepPi   --  checking count and range"
echo "        --  should click into extremes"

echo
echo "sample:"
hgsql -t -e "SELECT * from pepPi limit 1" $db
echo
hgsql -t -e "SELECT COUNT(*) from pepPi" $db
echo
set var=`hgsql -e "SELECT AVG(pI) FROM pepPi" $db`
hgsql -t -e "SELECT AVG(pI) FROM pepPi" $db
echo

echo "extremes of pI:"
echo
echo "pepPi three lowest:"
hgsql -t -e "SELECT * from pepPi ORDER BY pI LIMIT 3" $db
echo

echo "pepPi three highest:"
hgsql -t -e "SELECT * from pepPi ORDER BY pI DESC LIMIT 3" $db
echo



# --------------------------------------------
# pepMwAa   --  check count and range
#           --  should click into extremes

echo
echo "pepMwAa   --  check count and range"
echo "          --  should click into extremes"



echo
echo "sample:"
hgsql -t -e "SELECT * from pepMwAa limit 1" $db
echo
hgsql -t -e "SELECT COUNT(*) from pepMwAa" $db
echo
# echo "average MW:"
hgsql -t -e "SELECT ROUND(AVG(molWeight)) from pepMwAa" $db 
echo
# echo "min MW:"
hgsql -t -e "SELECT MIN(molWeight) from pepMwAa" $db
echo
# echo "max MW:"
hgsql -t -e "SELECT MAX(molWeight) from pepMwAa" $db
echo
# echo "average aaLen:"
hgsql -t -e "SELECT AVG(aaLen) from pepMwAa" $db
echo
# echo "min aaLen:"
hgsql -t -e "SELECT MIN(aaLen) from pepMwAa" $db
echo
# echo "max aaLen:"
hgsql -t -e "SELECT MAX(aaLen) from pepMwAa" $db
echo


# --------------------------------------------
# get count and one record from each (except for x,y tables above)

echo
echo "get one record from each (except for x,y tables above)"
comm -23 $tablelist AAlist1 > List3 
comm -23 List3 pepList > List4 
foreach table (`cat List4`)
  set c=`hgsql -N -e "SELECT COUNT(*) FROM $table" $db`
  echo "$table (count): $c"
  hgsql -t -e "SELECT * FROM $table LIMIT 1" $db
  echo
end
echo

# --------------------------------------------
# show index from each (except for x,y tables above)

echo
echo "show index from each (except for x,y tables above)"
cat $tablelist | grep -v "pbAaDist" > List5 
# echo "checking List5"
foreach table (`cat List5`)
  # echo "$table"
  hgsql -t -e "SHOW INDEX FROM $table" $db
  echo
end
echo


# --------------------------------------------
# check for null or zero values

echo
echo "check for null or zero values"
echo
foreach table (`cat $tablelist`)
  hgsql -N -e "DESC $table" $db | awk '{print $1}' > ${table}Cols
  echo $table
  echo "============="
  set totRows=`hgsql -N -e  'SELECT COUNT(*) FROM '$table'' $db`
  echo "total rows = "$totRows
  rm -f $table.nullzero.totals
  echo "null + zero entries:"
  foreach col (`cat ${table}Cols`)
    set cnt=`hgsql -N -e  'SELECT COUNT(*) FROM '$table' WHERE '$col='""' $db`
    echo "$col\t$cnt"
  end
  if ($table == "ensGeneXref") then
    echo "expect translation_name col to be empty due to ensembl change"
    echo
  endif
  echo
end
echo

# --------------------------------------------
# sfDes    -- look for uniq

echo
echo "sfDes    -- look for uniq"


echo
hgsql -e "SELECT COUNT(*) FROM sfDes" $protDb
hgsql -e "SELECT COUNT(DISTINCT(description)) FROM sfDes" $protDb
hgsql -e "SELECT COUNT(DISTINCT(classification)) FROM sfDes" $protDb
echo


#--------------------------------------------
echo "$protDb.interProXref --  check index"
echo "                            --  count rows"
echo "                            --  look for empty fields"

hgsql -t -e "SELECT * FROM interProXref limit 1" $protDb
hgsql -t -e "SHOW INDEX FROM interProXref" $protDb
hgsql -t -e "SELECT COUNT(*) FROM interProXref" $protDb

echo

echo end

rm -f *Cols

#! /bin/tcsh
source `which qaConfig.csh`


############################
#
#  04-26-04
#  Compare the content dev and beta trackDb tables
#  Modified from Bob Kuhn's doublecheck.csh
#  May specify different DB (versions) on hgwdev and hgwbeta
#
############################

if ($#argv != 2) then
  echo
  echo "  checks contents of trackDb tables on dev and beta and reports the diff"
  echo
  echo "  usage: trackDb_compare.csh <db1_release> <db2_release>"
  echo
  exit 1
endif

set db1 = $argv[1]
set db2 = $argv[2]
set table = "trackDb"

echo "using new db = " $db1
echo "using old db = " $db2

# --------------------------------------------
# get contents of each table and diff

echo
echo "Extract $table and visibility fields"
echo
hgsql -N -e "SELECT tableName, visibility FROM $table" $db1 | sort > $db1.$table.visibility.comp1.sort.txt
hgsql -N -e "SELECT tableName, visibility FROM $table" $db2 | sort > $db2.$table.visibility.comp1.sort.txt

# Determine lines uniq 
comm -23 $db1.$table.visibility.comp1.sort.txt $db2.$table.visibility.comp1.sort.txt > unique_new.txt
comm -13 $db1.$table.visibility.comp1.sort.txt $db2.$table.visibility.comp1.sort.txt > unique_old.txt

echo "Unique to $db1"
echo "-----------------"
cat unique_new.txt
echo
echo "Unique to $db2"
echo "-----------------"
cat unique_old.txt


#! /bin/tcsh
source `which qaConfig.csh`


############################
#
#  04-22-04
#  Compare the content dev and beta trackDb tables
#  Modified from Bob Kuhn's doublecheck.csh
#  May specify different DB (versions) on hgwdev and hgwbeta
#
############################

if ($#argv != 2) then
  echo
  echo "  checks contents of trackDb tables on dev and beta and reports the diff"
  echo
  echo "  usage: trackDb_compare.csh <dev_db_release> <beta_db_release>"
  echo
  exit 1
endif

set db_dev = $argv[1]
set db_beta = $argv[2]
set table = "trackDb"

echo "using hgwdev db = " $db_dev
echo "using hgwbeta db = " $db_beta
echo

# --------------------------------------------
# get contents of each table and diff

echo
echo "Compare tableName, shortLabel and type fields"
hgsql -N -e "SELECT tableName, shortLabel, type FROM $table" $db_dev | sort > $db_dev.$table.hgwdev.comp1.sort.txt
hgsql -h $sqlbeta -N -e "SELECT tableName, shortLabel, type FROM $table" $db_beta | sort > $db_beta.$table.hgwbeta.comp1.sort.txt

# Determine lines uniq to dev and beta
set uniqDev=`comm -23 $db_dev.$table.hgwdev.comp1.sort.txt $db_beta.$table.hgwbeta.comp1.sort.txt | wc -l`
set uniqBeta=`comm -13 $db_dev.$table.hgwdev.comp1.sort.txt $db_beta.$table.hgwbeta.comp1.sort.txt | wc -l`

# Select the tableName only
gawk '{ print $1 }' $db_dev.$table.hgwdev.comp1.sort.txt > $db_dev.$table.hgwdev.comp1.names_only.sort.txt
gawk '{ print $1 }' $db_beta.$table.hgwbeta.comp1.sort.txt > $db_beta.$table.hgwbeta.comp1.names_only.sort.txt

# Determine those tableNames which are unique
comm -23 $db_dev.$table.hgwdev.comp1.names_only.sort.txt $db_beta.$table.hgwbeta.comp1.names_only.sort.txt > names_only_in_dev.txt 
comm -13 $db_dev.$table.hgwdev.comp1.names_only.sort.txt $db_beta.$table.hgwbeta.comp1.names_only.sort.txt > names_only_in_beta.txt

# Print the uniq tableName fields
echo
echo "tableName which are unique to $db_dev on hgwdev"
echo "Total number of  unique tableName"
wc -l names_only_in_dev.txt
echo
cat names_only_in_dev.txt
echo
echo "tableName which are unique to $db_beta on hgwbeta"
echo "Total number of  unique tableName"
wc -l names_only_in_beta.txt
cat names_only_in_beta.txt
echo


if ($uniqDev != 0 || $uniqBeta != 0) then
	echo "  hgdev $db_dev and hgwbeta $db_beta tableName, shortLabel and type fields do not match: "
	echo "  $uniqDev uniq to dev"
	echo "  $uniqBeta uniq to beta"
	echo
	echo "diff the two following files to see differences"
	echo "diff $db_dev.$table.hgwdev.comp1.sort.txt $db_beta.$table.hgwbeta.comp1.sort.txt"
	else
	echo "$db_dev.$table.hgwdev.comp1.sort.txt and $db_beta.$table.hgwbeta.comp1.sort.txt are the same"

endif

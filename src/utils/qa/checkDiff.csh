#! /bin/tcsh


############################
#
#  04-21-04
#  check that dev and beta tables have same contents
#
############################

set db=""
set tablelist=""

if ($2 == "") then
  echo
  echo "  checks entire contents of tables on dev and beta and reports the diff."
  echo "  if data match, cleans out dump files."
  echo
  echo "    usage: database, tablelist (can be name of single table)."
  echo
  exit
else
  set db=$1
  set tablelist=$2
endif

echo
echo "using database $db"

if (! -e $2) then
  echo $2 > xxtablelistxx
  set tablelist="xxtablelistxx"
endif

# --------------------------------------------
# get contents of each table and diff

echo
foreach table (`cat $tablelist`)
  echo
  echo $table
  hgsql -N -e "SELECT * FROM $table" $db > $db.dev.$table
  hgsql -N -h hgwbeta -e "SELECT * FROM $table" $db > $db.beta.$table
  set uniqDev=`comm -23 $db.dev.$table $db.beta.$table | wc -l`
  set uniqBeta=`comm -13 $db.dev.$table $db.beta.$table | wc -l`
  if ($uniqDev != 0 || $uniqBeta != 0) then
    echo "  table does not match: "
    echo "  $uniqDev uniq to dev"
    echo "  $uniqBeta uniq to beta"
    echo "  see files: $db.dev.$table $db.beta.$table "
  else
    rm -f $db.dev.$table $db.beta.$table
    echo "  data match"
  endif
end
echo
rm -f xxtablelistxx

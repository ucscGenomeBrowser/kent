#!/bin/tcsh


###############################################
# 
#  05-13-08
#  Robert Kuhn
#
#  Finds all scaffolds or contigs that have no chain/net to any of
#    of the comparative tracks in otherOrgs
# 
###############################################


set db=""
set dbList=""
set count=""

if ($#argv != 2) then
  # no command line args
  echo
  echo "  finds all scaffolds or contigs that have no chain/net to any of the nets."
  echo
  echo "    usage:  database fileOfOtherDbNames"
  echo
  echo "    note:  expects db.net.OtherOrg output from chainNetTrio down one directory"
  echo
  exit
else
  set db=$argv[1]
  set dbList=$argv[2]
  if ( ! -e $dbList ) then
    echo "  ${dbList}: no such file"
    exit 1
  endif
endif

set count=`grep -c . $dbList`

foreach otherDb (`cat dbList`)
  set capDb=`echo $otherDb | perl -wpe '$_ = ucfirst($_)'`
  ls */$db.net.$capDb  | xargs grep 'is empty' | awk '{print $1}' > $otherDb.mtNet
end

echo
echo "number of empty nets:"
wc -l *mtNet | grep -v total
echo "number of chroms:"
hgsql -N -e "SELECT COUNT(*) FROM chromInfo" $db | tail -1
echo

echo "find nets that are empty in all nets:"
rm -f mtNetAll
foreach otherDb (`cat dbList`)
  cat $otherDb.mtNet >> mtNetAll
end
sort mtNetAll | uniq -c | sort -nr | head -10

echo
echo "number of chroms empty in all $count otherOrg nets:"
sort mtNetAll | uniq -c | sort -nr | grep -w $count | awk '{print $2}' > $db.mtNetList
wc -l $db.mtNetList | awk '{print $1}'
echo

echo "list of sizes of mtNets:"
hgsql -N -e "SELECT chrom, size FROM chromInfo" $db | grep -f $db.mtNetList  > $db.mtNetSizes
wc -l $db.mtNetSizes
echo
head -5 $db.mtNetSizes
echo "..."
tail -5 $db.mtNetSizes
echo

echo "total size:"
cat $db.mtNetSizes | awk '{total+=$2} END {print total/1000000}'
echo "(megabases)"

echo

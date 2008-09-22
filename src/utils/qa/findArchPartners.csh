#!/bin/tcsh

#######################
#
#  08-22-08
#  gets the names of all databases that have associations with a database
#
#######################

set db=""
set fullDbList=""

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ( $#argv != 1 ) then
  echo
  echo "  gets the names of all databases that have associations with a database"
  echo
  echo "    usage:  database"
  echo
  exit
else
  set db=$argv[1]
endif

# uppercase the db, make net name and find any assemblies with nets to db
set net=`echo $db| perl -wpe 's/(.*)/net\u$1/'`
getAssemblies.csh $net  | grep -v hgwbeta | tee $db.fullList

# get multiz tracks in other assemblies
echo "conservation:\n"
getConservation.csh $db | tee -a $db.fullList

# get liftOvers:
getLiftOver.csh $db hgwbeta | tee -a $db.liftOverList
echo

echo "------------"
# get hgGene connections (blastTab)
echo "knownGenes using ${db}:"
find ~/kent/src/hg/hgGene -name otherOrgs.ra | xargs grep $db \
  | awk -F":" '{print $1}' > $db.kgList
sed "s/\// /g" $db.kgList  | awk '{print $(NF-1)}' | sort 
echo

echo "------------"
echo "unique set of assemblies with tracks:"
cat $db.fullList | grep -v found | awk '{print $1}' | grep . | sort -u
echo

echo "------------"
echo "liftOver set:"
cat $db.liftOverList | grep -v hgwbeta | awk '{print $1}' | grep . | sort -u

exit 0


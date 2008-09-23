#!/bin/tcsh

#######################
#
#  08-22-08
#  gets the names of all databases that have associations with a database
#
#######################

set db=""
set fullDbList=""
set net=""

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
set Db=`echo $db | perl -wpe '$_ = ucfirst($_)'`
set net="net$Db"
getAssemblies.csh $net  | grep -v hgwbeta | tee $db.fullList

# get multiz tracks in other assemblies
echo "conservation:"
getConservation.csh $db | tee -a $db.fullList

# get {Human,Fly,Worm} Proteins tracks in other assemblies
getAssemblies.csh blast%${Db}% | egrep -v "found|split" | tee -a $db.fullList 

# find if {Human,Fly,Worm} Proteins track exists
echo "find if {Human,Fly,Worm} Proteins track exists.  ref to:"
hgsql -h hgwbeta -e "SHOW TABLES LIKE 'blast%'" $db | egrep "FB|KG|SG" \
  | egrep -v "Ref|Pep" \
  | sed "s/blast//" | perl -pwe '$_ = lcfirst($_)' | perl -wpe "s/FB|KG|SG//" \
  | tee -a $db.fullList
echo

# get liftOvers:
getLiftOver.csh $db hgwbeta | tee -a $db.liftOverList
echo

echo "------------"
# get hgGene connections (blastTab)
echo "knownGenes using $db (blastTab):"
find ~/kent/src/hg/hgGene -name otherOrgs.ra | xargs grep $db \
  | awk -F":" '{print $1}' > $db.kgList
sed "s/\// /g" $db.kgList  | awk '{print $(NF-1)}' | sort 
echo

echo "------------"
echo "liftOver set:"
cat $db.liftOverList | egrep -v "chains" | awk '{print $1}' | grep . | sort -u

echo
echo "------------"
echo "unique set of assemblies with connecting tracks:"
cat $db.fullList | egrep -v "blast|found" | awk '{print $1}' | grep . | sort -u
echo

rm -f $db.*List
exit 0


#!/bin/tcsh -e
source `which qaConfig.csh`

########################################
# 
# Ann Zweig 05-2009
#
# Use this script to create a file for the folks at UnitProt.
# They will use the file to create links from their web site back to our
# Gene details pages.
#
########################################

onintr cleanup

set db=''
set org=''
set hasKGs=''
set num=''

if ($#argv != 1 ) then
 echo
 echo "  Use this script to create a mapping of UniProt IDs to Gene IDs. \
  UniProt will pick it up from our download server and use it to \
  create links from their web site to our gene details pages. For the \
  gene sets from outside sources, check this script first to be sure it is \
  using the most up-to-date tables."
 echo
 echo "    usage: db"
 echo
 exit 1
else
 set db=$argv[1]
endif

# run only from hgwdev
if ( "$HOST" != "hgwdev" ) then
 echo "\nERROR: you must run this script on hgwdev!\n"
 exit 1
endif

# human and mouse use an official UCSC Genes build
if ( $db =~ "hg*" || $db =~ "mm*" ) then
 # get the data from the two KG-related tables
 hgsql -Ne "SELECT displayId, kgId FROM kgProtAlias" \
  $db > $db.rawDataForUniProt
 hgsql -Ne "SELECT spId, kgId FROM kgSpAlias WHERE spId != ''" \
  $db >> $db.rawDataForUniProt

else #non-UCSC Gene assembly
 # each of the non-KG assemblies are treated a little differently

 if ( $db =~ "rn*" ) then
  hgsql -Ne "SELECT value, name FROM rgdGene2ToUniProt" $db > $db.rawDataForUniProt
 endif

 if ( $db =~ "dm*" ) then
  hgsql -Ne "SELECT alias, a.name FROM flyBase2004Xref AS a, \
   flyBaseToUniProt AS b WHERE a.name=b.name AND alias != 'n/a'" \
   $db > $db.rawDataForUniProt 
 endif

 if ( $db =~ "ce*" ) then
  hgsql -Ne "SELECT acc, name FROM sangerGene AS a, uniProt.gene AS b \
   WHERE a.proteinID=b.val and acc != 'n/a'" $db > $db.rawDataForUniProt
 endif
 
 if ( $db =~ "danRer*" ) then
  echo " \nERROR: Although a file could be generated for danRer, uniProt has"
  echo " decided that they do not want to do that mapping...yet\n" 
  exit 1
 endif
 
 if ( $db =~ "sacCer*" ) then
  echo " \nERROR: Although a file could be generated for sacCer, uniProt has"
  echo " decided that they do not want to do that mapping...yet\n" 
  exit 1
 endif
endif

if ( -e $db.rawDataForUniProt ) then 
 # find out the name of the organism for this database
 set org=`hgsql -Ne 'SELECT organism FROM dbDb where name = "'$db'" LIMIT 1' \
  hgcentraltest | perl -wpe '$_ = lcfirst($_)'`

 # now add the organism name to every row
 sed -e 's/$/ '$org'/' $db.rawDataForUniProt > $db.rawDataForUniProt.plus

else
  echo " \nERROR: It is not possible to make a mapping file for UniProt from"
  echo " the database you entered: $db\n"
  exit 1
endif

# make sure there is only one Gene ID - UniProt ID pair
sort -k1,1 -u $db.rawDataForUniProt.plus > $db.uniProtToUcscGenes.txt

# make the new directory and copy the file there
set copyDir = /usr/local/apache/htdocs-hgdownload/goldenPath/$db/UCSCGenes
mkdir -p $copyDir
mv $db.uniProtToUcscGenes.txt $copyDir/uniProtToUcscGenes.txt

# how big is the file
set num=`wc -l $copyDir/uniProtToUcscGenes.txt | awk '{print $1}'`


# explain the output to the user
echo "\nSUCCESS!\n"
echo "Here's a sample of the ${num}-line file you just created"
echo " (expect: UniProtId ucscGeneId orgName)"
head $copyDir/uniProtToUcscGenes.txt
echo " \nAsk for a push of your new file to hgdownload:\n"
echo " /usr/local/apache/htdocs-hgdownload/goldenPath/$db/UCSCGenes/uniProtToUcscGenes.txt"
# clean up old files (except the real one)
cleanup:
rm -f $db.rawDataForUniProt*

exit 0

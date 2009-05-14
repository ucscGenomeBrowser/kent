#!/bin/tcsh
source `which qaConfig.csh`

########################################
# 
# Ann Zweig 5-2009
#
# Use this script to create a file for the folks at UnitProt.
# They will use the file to create links from their web site back to our
# UCSC Gene details pages.
#
########################################

set db=''
set org=''
set hasKGs=''
set num=''

if ($#argv != 1 ) then
 echo
 echo " Use this script to create a mapping of UniProt IDs to UCSC Gene ID."
 echo " UniProt will pick it up from our download server and use it to"
 echo " create links from their web site to our gene details pages."
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

# make sure this assembly has UCSC Genes
set hasKGs=`hgsql -Ne "SELECT genomeDb FROM gdbPdb" hgcentraltest | grep "$db"`
if ( '' == $hasKGs ) then
 echo " \nERROR: this assembly does not appear to have a UCSC Genes track\n"
 exit 1
endif

# get the data from the two tables
hgsql -Ne "SELECT displayId, kgId FROM kgProtAlias" $db > $db.rawDataForUniProt
hgsql -Ne "SELECT spId, kgId FROM kgSpAlias WHERE spId != ''" $db >> $db.rawDataForUniProt

# make sure there is only one UCSC Gene ID - UniProt ID pair
sort -k1,1 -u $db.rawDataForUniProt > $db.rawDataForUniProt.sorted

# find out the name of the organism for this database
set org=`hgsql -Ne 'SELECT organism FROM dbDb where name = "'$db'" LIMIT 1' hgcentraltest\
 | perl -wpe '$_ = lcfirst($_)'`

# now add the organism name to every row
cat $db.rawDataForUniProt.sorted | sed -e 's/$/ '"$org"'/g' > $db.uniProtToUcscGenes.txt

# make the new directory and copy the file there
mkdir -p /usr/local/apache/htdocs/goldenPath/$db/UCSCGenes
cp $db.uniProtToUcscGenes.txt /usr/local/apache/htdocs/goldenPath/$db/UCSCGenes/uniProtToUcscGenes.txt

# how big is the file
set num=`cat $db.uniProtToUcscGenes.txt | wc -l`

# explain the output to the user
echo "\nSUCCESS!\n"
echo "Here's a sample of the $num line file you just created"
echo " (expect: UniProtId ucscGeneId orgName)"
head $db.uniProtToUcscGenes.txt
echo " \nAsk for a push of your new file to hgdownload:\n"
echo " /usr/local/apache/htdocs/goldenPath/$db/UCSCGenes/uniProtToUcscGenes.txt"

# clean up old files (except the real one)
rm $db.rawDataForUniProt*

exit 0

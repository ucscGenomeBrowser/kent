#!/bin/tcsh
source `which qaConfig.csh`

# to do:  check that trackDb.hgGene is "on"
# to do:  see that cgapBiocDesc has unique entries

# --------------------------------------------
#  run HGGeneCheck.java on dev:

#### echo
#### echo " --------------------------------------------"
####
#### echo "server hgwdev.soe.ucsc.edu /
#### machine hgwdev.soe.ucsc.edu /
#### quick false /
#### dbSpec $db /
#### table all /
#### zoomCount 4" > $db.props
###
#### echo "run HGGeneCheck: \
####   nohup nice HGGeneCheck $db.props > & $db.HGrobot.out & "
#### echo
#echo " --------------------------------------------"

###############################################
# 
#  03-09-04
#  Robert Kuhn
#
#  This script runs through a number of checks on the Known Genes tables.  
#  You need to run HGGeneCheck first. Also you need to have 3 files 
#  (kgTables, gsTables, and pbTables) that list the tables involved one
#  directory above the directory where you choose to run knownGene.csh.
#  
#  run HGGeneCheck.java on dev: 
#  nohup nice HGGeneCheck $db.props > & $db.HGrobot.out &
# 
###############################################

set db=""
set oldDb=""
set trackName="kg"
set table="knownGene"


if ($#argv == 0 || $#argv > 2) then
  # no command line args
  echo
  echo "  Runs test suite on Known Genes track."
  echo "  Before running this script run HGGeneCheck in the same"
  echo "  directory where you intend to run knownGene.csh ."
  echo "  Note that this script generates many output files."
  echo "  "
  echo "    usage:  database1 [database2] Checks db1 on dev and db2 on beta." 
  exit
else
  set db=$argv[1]
endif

if ($#argv == 2) then
  set oldDb=$argv[2]
else
  set oldDb=$db
endif


echo "using $db for dev db"
echo "using $oldDb for beta db "
set curr_dir=$cwd

#  get sets of KG, GS, PB tables from master list:
findKgTableSet.csh $db 

# count items in list
echo "found the following KG tables:"
wc -l $db.{kg,gs,pb}Tables
cat $db.{kg,gs,pb}Tables | sort -u > $db.allTables
echo "will have some overlap. uniq tables = `cat $db.allTables | sort -u | wc -l`"

# find new tables: in pushQ but NOT in a kgTables list
echo
echo "find new tables: in pushQ but NOT in a kgTables list"
hgsql -h $sqlbeta -Ne "SELECT tbls FROM pushQ WHERE tbls LIKE '%knownGene%' \
  AND dbs = '"$db"'" qapushq | sed "s/\\n/\n/g" | egrep -r . > $db.pushqKgList
dos2unix $db.pushqKgList >& /dev/null
commTrio.csh $db.allTables $db.pushqKgList 

if ( `wc -l $db.allTables.Only | awk '{print $1}'` != 0 ) then
  echo "these tables may be in separate tracks? ($db.allTables.Only) :"
  cat $db.allTables.Only
endif 
echo

# --------------------------------------------

echo
echo "compare new genes to old set (uniq):"
echo "not useful for first releasse of KG3-type track"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
hgsql -N -e "SELECT name from knownGene" $db | sort | uniq > $db.KG.name.dev
hgsql -N -h $sqlbeta -e "SELECT name from knownGene" $oldDb | sort |uniq > $oldDb.KG.name.beta.uniq
comm -23 $db.KG.name.dev $oldDb.KG.name.beta.uniq > $db.KG.name.devOnly
comm -13 $db.KG.name.dev $oldDb.KG.name.beta.uniq > $oldDb.KG.name.betaOnly
comm -12 $db.KG.name.dev $oldDb.KG.name.beta.uniq > $db.KG.name.common
wc -l  $db.KG.name.devOnly $oldDb.KG.name.betaOnly $db.KG.name.common | grep -v total 

echo
echo "check some new ones by hand:"
head -20 $db.KG.name.devOnly | tail -3
echo
echo "check some old ones by hand on beta:"
head -20 $oldDb.KG.name.betaOnly | tail -3
echo

echo "strip version number and compare again:"
awk -F. '{print $1}' $db.KG.name.devOnly     > $db.KG.name.devOnly.noVersion 
awk -F. '{print $1}' $oldDb.KG.name.betaOnly > $oldDb.KG.name.betaOnly.noVersion
commTrio.csh  $db.KG.name.devOnly.noVersion $oldDb.KG.name.betaOnly.noVersion

# --------------------------------------------
# check alignID for unique:

echo
echo "checking alignID for unique:"
hgsql -N -e "SELECT alignID FROM knownGene" $db | sort > $db.KG.alignID.sort
uniq  $db.KG.alignID.sort > $db.KG.alignID.uniq
comm -12  $db.KG.alignID.sort $db.KG.alignID.uniq > $db.KG.alignID.common
echo "expect all lines to be in common file:  all alignIDs are unique"

echo "if any are in sort file more than once:"
comm -23  $db.KG.alignID.sort $db.KG.alignID.uniq > $db.KG.alignID.dupes 
wc -l $db*alignID* | grep -v "total"
echo "three duplicate alignIDs (may be blank):"
head -3 $db.KG.alignID.dupes 
echo


# --------------------------------------------
# check strand:

echo "-------------------------------------------------"
echo
echo "counting strands:"
set totKG=`hgsql -N -e  'SELECT COUNT(*) FROM knownGene' $db`
set tot=`hgsql -N -e "SELECT COUNT(*) FROM knownGene" $db` 
set plus=`hgsql -N -e 'SELECT COUNT(*) FROM knownGene WHERE strand = "+"' $db` 
set minus=`hgsql -N -e 'SELECT COUNT(*) FROM knownGene WHERE strand = "-"' $db`
set null=`hgsql -N -e 'SELECT COUNT(*) FROM knownGene WHERE strand = ""' $db`

set total = `expr $null + $minus + $plus`

echo "null  = $null"
echo "plus  = $plus"
echo "minus = $minus"
echo "total = $total"
echo "rows  = $totKG"
echo


# -------------------------------------------------
# look for start and end order:

echo "-------------------------------------------------"
echo
echo "look for start and end order:"
rm -f txFile
set var1=`hgsql -N -e "SELECT COUNT(*) FROM knownGene WHERE txStart >= txEnd" $db` 
set var2=`hgsql -N -e "SELECT COUNT(*) FROM knownGene WHERE txStart > cdsStart" $db`
set var3=`hgsql -N -e "SELECT COUNT(*) FROM knownGene WHERE cdsEnd > txEnd" $db`
set var4=`hgsql -N -e "SELECT COUNT(*) FROM knownGene WHERE txStart = cdsStart" $db`
set var5=`hgsql -N -e "SELECT COUNT(*) FROM knownGene WHERE cdsEnd = txEnd" $db` 
set var6=`hgsql -N -e "SELECT COUNT(*) FROM knownGene WHERE txStart < 0" $db` 
set var7=`hgsql -N -e "SELECT COUNT(*) FROM knownGene WHERE cdsStart < 0" $db` 

echo
echo "txStart >= txEnd:   "$var1
echo "txStart > cdsStart: "$var2
echo "cdsEnd > txEnd:     "$var3
echo "txStart = cdsStart: "$var4" (means no 5'-UTR if + strand)"
echo "cdsEnd = txEnd:     "$var5" (means no 3'-UTR if + strand)"
echo "negative txStart:   "$var6
echo "negative cdsStart:  "$var7
echo

echo "-------------------------------------------------"
echo
echo "check if browser show no 5', 3'-UTR where predicted:"
echo "no 5'-UTR:"
hgsql -e "SELECT name, chrom, txStart, txEnd FROM knownGene \
         WHERE txStart = cdsStart LIMIT 3" $db
echo
echo "no 3'-UTR:"
hgsql -e "SELECT name, chrom, txStart, txEnd FROM knownGene \
         WHERE txEnd = cdsEnd LIMIT 3" $db


# -------------------------------------------------
# check ends:

echo "-------------------------------------------------"
# looking for off the end of each chrom (look for all positive values)
# looking for off-end of each chrom (look for all positive values)

echo
echo "looking for off the end of each chrom \
       (look for all positive values):"

checkOffend.csh $db $table
echo


# -------------------------------------------------
# check to see if there are genes on all chroms.


echo "-------------------------------------------------"
echo " check to see if there are genes on all chroms:"
echo
hgsql -N -e "SELECT ci.chrom, COUNT(*) AS genes \
  FROM chromInfo ci, knownGene kg WHERE ci.chrom=kg.chrom \
  AND ci.chrom NOT LIKE '%random' \
  GROUP BY ci.chrom ORDER BY chrom" $db
  
hgsql -N -e "SELECT ci.chrom, COUNT(*) AS genes \
  FROM chromInfo ci, knownGene kg WHERE ci.chrom=kg.chrom \
  AND ci.chrom LIKE '%random' \
  GROUP BY ci.chrom ORDER BY chrom" $db 
echo



echo "-------------------------------------------------"
echo " check coverage: "
echo
checkCoverage.csh $db $table

echo
runBits.csh $db knownGene
echo
runJoiner.csh $db knownGene

# -------------------------------------------------
# check exon sizes:
# 
# 
# # edit the list /cluster/home/heather/jdbc/data/tablelist
# echo
# echo "knownGene" > /cluster/home/heather/jdbc/data/KG
# 
# # and run /cluster/home/heather/jdbc/ExonSize.java:
# cd /cluster/home/heather/jdbc/
# source javaenv
# java ExonSize $db data/KG | egrep -v "items found|-----|Querying" > $curr_dir/$db.exonExtremes
# cd $curr_dir 
# 
# echo
# echo "number of very large or negative exons:"
# grep "exon" $db.exonExtremes | wc -l
# # echo "five lines are kruft (filtered above now with grep -v"
# echo
# echo "example of very large or negative exons:"
# head -5 $db.exonExtremes
# echo

# -------------------------------------------------
# count exons

echo
echo "count exons"
echo "genes with the most exons:"
hgsql -t -e "SELECT name, exonCount FROM knownGene ORDER BY exonCount DESC limit 5" $db > $db.manyExons
cat $db.manyExons
echo

# -------------------------------------------------
# look for unique names 

echo
echo "look for unique names:" 
hgsql -N -e "SELECT name FROM knownGene" $db | sort \
  > $db.KG.name.sort
uniq $db.KG.name.sort > $db.KG.name.uniq 

set kgSort=`wc -l $db.KG.name.sort`
set kgUniq=`wc -l $db.KG.name.uniq`
set diff=`comm -23 $db.KG.name.sort $db.KG.name.uniq \
    | wc -l`
echo "sort: "$kgSort
echo "uniq: "$kgUniq
echo "diff: "$diff
echo


# ------------------------------------

# todo: get list of genes and their geneSymbol and count up how
# many have the same name.  look at outliers.  what follows here doesn't
# work anymore as KG.name is now a unique field.
# todo: analyze knownCanonical clusters


# # delete lines with single hit "   1[tab]" 
# # -- and count lines remaining (genes used more than once):
# 
# echo
# sort  $db.KG.name.sort | uniq -c | sort -nr > $db.KG.name.uniq.most
# 
# echo "number of genes that align more than once:"
# sed -e "/  1 /d" $db.KG.name.uniq.most | wc -l
# echo
# 
# echo "gene occurance profile"
# echo
# textHistogram -col=1 $db.KG.name.uniq.most
# rm file
# 
# echo
# echo "genes that align the most times:"
# head $db.KG.name.uniq.most
# echo



# -------------------------------------------------
# examine overlapping genes ???
#   .../jdbc/Overlap.java
#   how ???  There are many
# -------------------------------------------------
# look for same number of rows in kgXref, knownGenePep, knownGeneMrna

echo
echo "-------------------------------------------------"
echo "look for same number of rows in kgXref, knownGenePep, knownGeneMrna:"
echo "kgXref"
set xrefNum=`hgsql -N -e "SELECT COUNT(*) FROM kgXref" $db`
set xrefNumUniq=`hgsql -N -e "SELECT COUNT(DISTINCT(kgID)) FROM kgXref" $db`
echo "$xrefNum   $xrefNumUniq (uniq)"
echo "knownGenePep"
set kgPep=`hgsql -N -e "SELECT COUNT(*) FROM knownGenePep" $db`
echo $kgPep
echo "knownGeneMrna"
set kgMrna=`hgsql -N -e "SELECT COUNT(*) FROM knownGeneMrna" $db`
echo $kgMrna
echo "should match uniqs in KG, except for fewer Peps, now that we include non-coding:"
wc -l $db.KG.name.uniq
echo

echo "dupes in kgXref.kgID field:"
if ($xrefNum != $kgPep) then
  hgsql -e "SELECT kgID, COUNT(*) as number FROM kgXref GROUP BY kgID \
    ORDER BY number DESC LIMIT 5 " $db
endif
echo "should be no dupes anymore"

# -------------------------------------------------
# check that names in knownGene match names in knownGenePep, knownGeneMrna
# check that names in knownGene match kgID in kgXref
# check that names in knownGene match uniq kgID in kgProtAlias

echo

hgsql -N -e "SELECT name FROM knownGene" $db | sort | uniq > $db.KG.name.uniq
hgsql -N -e "SELECT name FROM knownGenePep" $db | sort | uniq > $db.KGpep.name.uniq
hgsql -N -e "SELECT name FROM knownGeneMrna" $db | sort | uniq > $db.KGrna.name.uniq
hgsql -N -e "SELECT kgID FROM kgXref" $db | sort | uniq > $db.kgXref.kgID.uniq
hgsql -N -e "SELECT kgID FROM kgProtAlias" $db | sort  | uniq > $db.kgProtAlias.kgID.uniq



echo "-------------------------------------------------"
echo "check that names in knownGene match names in knownGenePep, knownGeneMrna:"
diff $db.KG.name.uniq $db.KGpep.name.uniq | wc -l
diff $db.KG.name.uniq $db.KGrna.name.uniq | wc -l
echo "expect zero. upper number is number of genes without pep"
echo

echo "-------------------------------------------------"
echo "check that names in knownGene match kgID in kgXref and kgProtAlias:"
# diff $db.KG.name.uniq $db.kgXref.kgID.uniq | wc -l
# diff $db.KG.name.uniq $db.kgProtAlias.kgID.uniq | wc -l

echo "uniq to KG.name (not in kgXref.kgID):"
comm -23 $db.KG.name.uniq $db.kgXref.kgID.uniq | wc -l
echo "uniq to kgXref.kgID: (not in KG.name):"
comm -13 $db.KG.name.uniq $db.kgXref.kgID.uniq | wc -l

echo "uniq to KG.name (not in kgProtAlias.kgID):"
comm -23 $db.KG.name.uniq $db.kgProtAlias.kgID.uniq | wc -l
echo "uniq to kgProtAlias.kgID: (not in KG.name):"
comm -13 $db.KG.name.uniq $db.kgProtAlias.kgID.uniq | wc -l

echo "expect zero"
echo "as of KGII, duplicate kgXref.kgID are ok.  will be corrected in KGIII."
echo

echo "number of non-coding genes in kgTxInfo:"
hgsql -e ' SELECT category, COUNT(*) AS number FROM kgTxInfo GROUP BY category \
  ORDER BY number DESC' $db
echo "total non-coding:"
hgsql -e 'SELECT COUNT(*) FROM kgTxInfo WHERE category NOT LIKE "coding"' $db



# -------------------------------------------------
# knownGenePep, knownGeneMrna -- check numbers of entries vs old version
# knownGenePep, knownGeneMrna -- check min and max lengths of seq column

echo
echo "-------------------------------------------------"
echo "knownGenePep, knownGeneMrna -- check numbers of entries vs old version:"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
echo "dev first"
echo
echo "Pep"
hgsql -N -e "SELECT COUNT(*) FROM knownGenePep" $db
hgsql -h $sqlbeta -N -e "SELECT COUNT(*) FROM knownGenePep" $oldDb
echo
echo "Mrna"
hgsql -N -e "SELECT COUNT(*) FROM knownGeneMrna" $db
hgsql -h $sqlbeta -N -e "SELECT COUNT(*) FROM knownGeneMrna" $oldDb
echo "should be some new ones"
echo


echo "knownGenePep, knownGeneMrna -- check min and max lengths of seq column:"
echo "dev, format: {MIN, MAX}Mrna, {MIN, MAX}Pep:"
echo "----"
 hgsql -N -e "SELECT MIN(LENGTH(seq)) FROM knownGeneMrna" $db
 hgsql -N -e "SELECT MAX(LENGTH(seq)) FROM knownGeneMrna" $db
 hgsql -N -e "SELECT MIN(LENGTH(seq)) FROM knownGenePep" $db
 hgsql -N -e "SELECT MAX(LENGTH(seq)) FROM knownGenePep" $db

echo
echo "beta, format: {MIN, MAX}Mrna, {MIN, MAX}Pep:"
echo "----"
 hgsql -h $sqlbeta -N -e "SELECT MIN(LENGTH(seq)) FROM knownGeneMrna" $oldDb
 hgsql -h $sqlbeta -N -e "SELECT MAX(LENGTH(seq)) FROM knownGeneMrna" $oldDb
 hgsql -h $sqlbeta -N -e "SELECT MIN(LENGTH(seq)) FROM knownGenePep" $oldDb
 hgsql -h $sqlbeta -N -e "SELECT MAX(LENGTH(seq)) FROM knownGenePep" $oldDb
echo "sometimes the same on dev as beta:  no new proteins at extremes "
echo

echo  "-------------------------------------------------"
# kgXref -- mrna generally equals kgID

echo "kgXref -- mrna generally equals kgID"
echo "count:"
hgsql -N -e "SELECT COUNT(*) FROM kgXref" $db
echo "count mrna != kgID:"
hgsql -N -e "SELECT COUNT(*) FROM kgXref WHERE mrna != kgID" $db
echo "expect zero"
echo


echo  "-------------------------------------------------"
# kgXref -- empty fields
# should give zero (no empty fields):

echo "refseq column can be empty (protAcc is, too, then)"
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE refseq = "" ' $db
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE refseq = "" AND protAcc = "" ' $db
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE refseq = "" AND protAcc != "" ' $db
echo "expect last value to be zero"
echo


# check all tables for null + zero entries:

echo
echo  "-------------------------------------------------"
echo "check all tables for null + zero entries:"
cat kgTables gsTables pbTables | sort -u > kgTablesAll
foreach table (`cat kgTablesAll`)
  hgsql -N -e "DESC $table" $db | gawk '{print $1}' > ${table}Cols
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
  echo
end
echo


# check all tables for null entries:

echo
foreach table (`cat kgTablesAll`)
  hgsql -N -e "DESC $table" $db | gawk '{print $1}' > ${table}Cols
  echo $table
  echo "============="
  set totRows=`hgsql -N -e  'SELECT COUNT(*) FROM '$table'' $db`
  echo "total rows = "$totRows
  rm -f $table.null.totals
  echo "null entries:"
  foreach col (`cat ${table}Cols`)
    set cnt=`hgsql -N -e  "SELECT COUNT(*) FROM $table WHERE $col IS NULL" $db`
    echo "$col\t$cnt"
  end
  echo
end
echo


echo
echo  "-------------------------------------------------"
echo "check all tables for null + zero entries:"
echo "some, but not all  of protAcc column will be empty:"
echo "kgXref.protAcc empty:"
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE protAcc = "" ' $db
echo "kgXref.protAcc not empty:"
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE protAcc != "" ' $db
echo


# -------------------------------------------------
# kgXref -- can have duplicate values in spID


echo  "-------------------------------------------------"
echo "kgXref -- can have duplicate values in spID"
set tot=`hgsql -N -e "SELECT COUNT(*) FROM kgXref" $db` 
hgsql -N -e "SELECT spID FROM kgXref" $db | sort | uniq > $db.kgXref.spID.uniq
set uniq=`hgsql -N -e "SELECT spID FROM kgXref" $db | sort | uniq | wc -l`
set diff = `expr $tot - $uniq`
echo "tot   = "$tot
echo "uniq  = "$uniq
echo "dupes = "$diff
echo


# -------------------------------------------------
# kgXref -- can position-search on spID:   
#   make a java robot

echo  "-------------------------------------------------"
echo "kgXref -- can position-search on spID:"
echo "check manually"
hgsql -N -e "SELECT spID FROM kgXref ORDER BY spID DESC limit 2" $db
echo


# -------------------------------------------------
# kgAlias -- check kgAlias.alias for uniq.  search position box with some

echo  "-------------------------------------------------"
echo "kgAlias -- check kgAlias.alias for uniq.  search position box with some."
echo
echo "kgAlias count:"
hgsql -N -e "SELECT COUNT(*) FROM kgAlias" $db 
echo "uniq:"
hgsql -N -e "SELECT alias FROM kgAlias" $db | sort | uniq > $db.kgAlias.alias.uniq
wc -l $db.kgAlias.alias.uniq
echo "expect more uniq than KG names, but many dupes"
echo
echo "get some records"
hgsql -e "SELECT alias, kgID FROM kgAlias ORDER BY alias limit 5" $db 
echo


# -------------------------------------------------
# kgAlias -- uniq IDs from KG can have one or more matches

echo  "-------------------------------------------------"
echo "kgAlias -- uniq IDs from KG can have one or more matches"
hgsql -N -e "SELECT name FROM knownGene" $db | sort | uniq > $db.KG.name.uniq
wc -l  $db.KG.name.uniq
hgsql -N -e "SELECT kgID FROM kgAlias" $db | sort | uniq > $db.kgAlias.kgID.uniq

comm -23 $db.KG.name.uniq $db.kgAlias.kgID.uniq | wc -l
comm -13 $db.KG.name.uniq $db.kgAlias.kgID.uniq | wc -l
comm -12 $db.KG.name.uniq $db.kgAlias.kgID.uniq | wc -l
echo "expect 0, 0, all"
echo


# -------------------------------------------------
# kgAlias -- kgAlias uniq count should be less than KG.uniq

echo
echo  "-------------------------------------------------"
echo "kgAlias -- kgAlias uniq count should be less than or equal to KG.uniq"
hgsql -N -e "SELECT kgID FROM kgAlias" $db | sort | uniq | wc -l
hgsql -N -e "SELECT name FROM knownGene" $db | sort | uniq | wc -l

hgsql -N -e "SELECT kgID FROM kgAlias" $db | sort | uniq  > $db.kgAlias.kgID.uniq
diff $db.KG.name.uniq $db.kgAlias.kgID.uniq | wc -l
echo "expect zero"
echo


# -------------------------------------------------
# kgAlias -- expect multiple rows with same kgID value

echo
echo  "-------------------------------------------------"
echo "kgAlias -- expect multiple rows with same kgID value"
set tot=`hgsql -N -e "SELECT COUNT(*) FROM kgAlias" $db`
set uniq=`hgsql -N -e "SELECT kgID FROM kgAlias" $db | sort | uniq | wc -l`
set diff = `expr $tot - $uniq`
echo "tot   = "$tot
echo "uniq  = "$uniq
echo "dupes = "$diff
echo

# -------------------------------------------------
# cgapAlias -- check that at least some cgapAlias.alias 
#    entries are in knownGene.name

echo
echo  "-------------------------------------------------"
echo "cgapAlias -- check that at least some cgapAlias.alias \
  entries are in knownGene.name"
hgsql -N -e "SELECT name FROM knownGene" $db | sort | uniq > $db.KG.name.uniq
hgsql -N -e "SELECT alias FROM cgapAlias" $db | sort | uniq > $db.cgapAlias.alias.uniq
wc -l $db.cgapAlias.alias.uniq | grep -v total

comm -23 $db.KG.name.uniq $db.cgapAlias.alias.uniq  > $db.KG.notin.cgap
comm -12 $db.KG.name.uniq $db.cgapAlias.alias.uniq  > $db.KG.in.cgap
echo 
wc -l $db*KG*cgap* 
echo  "expect total be number of KG"
echo


# -------------------------------------------------
# keggPathway, keggMapDesc -- validate
#  automate this w/ httpunit.  

echo
echo  "-------------------------------------------------"
echo "keggPathway, keggMapDesc -- validate"
echo
hgsql -e "SELECT * FROM keggPathway limit 5" $db > $db.kegg
cat $db.kegg

echo
# get the mapID for these 5:
echo "check some by hand, using kgID -- should have kegg section \
       at bottom of KG page"

foreach mapID (`cat $db.kegg | gawk '{print $3}'`)
  hgsql -N -e 'SELECT * FROM keggMapDesc WHERE mapID = "'$mapID'"' $db
end
   

echo
# hgsql -e "SELECT * FROM keggMapDesc limit 5" $db
echo


# ooooooooooooooo  add logic for the following:
# -------------------------------------------------
# check KG sort
# confirm the new table has the same rows as the old
#  (sorted but not uniqed)

echo
echo  "-------------------------------------------------"
echo "checking for KG position sorting:"
hgsql -N -e "SELECT chrom FROM chromInfo" $db > $db.chroms
foreach chrom (`cat $db.chroms`)
  hgsql -N -e 'SELECT txStart FROM knownGene WHERE chrom = "$chrom"' $db > $db.$chrom
  sort $db.$chrom > $db.$chrom.sort
  set sortDiff=`diff $db.$chrom $db.$chrom.sort | wc -l`
  if ($sortDiff > 0) then
    echo ""$chrom
  endif
end
echo "prints chrom name of any unsorted chrom.  expect none."
echo

# -------------------------------------------------
# check if KG is sorted by chrom:

# oooooooo fix sort

echo
# ~/bin/sortChromStart.csh $db knownGene
echo


# -------------------------------------------------
# start Comparative tests
# -------------------------------------------------


echo "-------------------------------------------------"
echo "          Comparative tests"
echo "-------------------------------------------------"


# ------------
# for all tables in list: (csh version):

echo 
echo  "-------------------------------------------------"
echo "compare description of all tables with previous"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
foreach table (`cat kgTablesAll`)
  hgsql -h $sqlbeta -N  -e "DESCRIBE $table" $oldDb >  $db.beta.$table.desc
  hgsql -N  -e "DESCRIBE $table" $db > $db.dev.$table.desc
  echo $table
  diff $db.dev.$table.desc $db.beta.$table.desc
  echo
end
echo

# find a way to count "<" and ">" lines in a file  
# "sed effort below didn't work

# hgsql -N -e 'SELECT name FROM knownGeneLink WHERE name = "NM_198864"' $db 
# found: therefore "<" in a diff means it is in the first file named
# hgsql -h $sqlbeta -N -e 'SELECT name FROM knownGeneLink \
#       WHERE name = "NM_198864"' $db 
# not found: therefore "<" in a diff means it is in the first file named

###############################
# didn't work:
# diff $db.dev.kgLink.sort $db.beta.kgLink.sort | sed -e '/>/' 
###############################
# didn't work:


echo
echo  "-------------------------------------------------"
echo "check rowcounts against beta"
echo "old version, beta,  listed first"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
foreach table (`cat kgTablesAll`)
  set old=`hgsql -h $sqlbeta -N  -e "SELECT COUNT(*) FROM $table" $oldDb`
  set new=`hgsql -N  -e "SELECT COUNT(*) FROM $table" $db`
  set newRows=`expr $new - $old`
  set percent = 0
  if ($newRows == 0) then
    echo
    echo "$table : same number of rows\n"
  else
    if ($newRows < 0) then
      set negRows=`echo $newRows | gawk '{print (-1) * $1}'`
      echo "$table has $negRows fewer rows"
    else
      echo "$table has $newRows more rows"
    endif
    echo "   old:  $old"
    echo "   new:  $new"
    if ($old != 0) then
      set percent=`echo $newRows $old | gawk '{printf "%.2f\n", $1/$2*100}'`
      # attempt to put a "+" in fornt of positive numbers.  except they are strings.
      if ($newRows > 0) then
        set percent="+"$percent
      endif
      echo "   diff: $newRows ("$percent"%)"
    endif
    echo
  endif
end
echo


###############################

# -------------------------------------------------
# kgAlias -- check diffs from former (two-field table):

echo
echo "kgAlias -- check diffs from former (two- field table):"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
hgsql -N -e "SELECT * FROM kgAlias" $db | sort > $db.dev.kgAlias.sort
hgsql -h $sqlbeta -N -e "SELECT * FROM kgAlias" $oldDb | sort > $oldDb.beta.kgAlias.sort
wc -l *kgAlias.sort | grep -v total
echo

# -------------------------------------------------
# some things on protocol not yet implemented:
# 
# comparative to previous asembly.  knownGene:
#   compare gene size.  examine changes clarger than 10K
#   compare exonCount.  examine changes larger than 5
# 
# comparative to previous assembly.  kgXref
#   use KGXrefCheck.java (only compares old assembly to new now.  won't do new assembly update)
#   use KGXrefDiff.java  
# 

# --------------------------------------------
#  check bioCyc tables:

echo
echo  "-------------------------------------------------"
echo  check bioCyc tables, if available:
echo
set biocyc=`cat {$trackName}TablesAll | grep "bioCyc" | wc -l`
if ($biocyc > 0) then
  echo "bioCycPathway"
  hgsql -N -e "SELECT COUNT(DISTINCT(kgID)) FROM bioCycPathway" $db
  hgsql -e "SELECT * FROM bioCycMapDesc LIMIT 5" $db

  echo
  echo "bioCycMapDesc"
  hgsql -N -e "SELECT COUNT(DISTINCT(mapID)) FROM bioCycMapDesc" $db
  hgsql -e "SELECT * FROM bioCycPathway LIMIT 5" $db
  echo
else
  echo "no bioCyc* tables in this assembly"
endif
echo
echo "flesh this out with checks on number of genes.  are any uniq, etc."
echo "pipe a column into awk and get the proper other values"
echo
echo "add support for check previous assembly as parameter if not a rebuild"

# -------------------------------------------------
echo "-------------------------------------------------"
echo "check for different types of knownAlt"
echo
hgsql -e "SELECT DISTINCT name, COUNT(*) AS number FROM knownAlt \
  GROUP BY name ORDER BY number DESC" $db
echo
 # maybe get a link for each type?



# -------------------------------------------------
# 
# further development:
#      -------------
# see ooooo above.
# comparatives:  compare gene size. examine changes larger than 10k bases
# comparatives:  compare exonCount.  examine changes larger than 5 
# use .../jdbc/KGXrefCheck 
#     --finds old db entries that do not match in new at kgID, spID or refseq.
# check kgAlias for added, deleted and changes aliases
# 
# -------------------------------------------------
# 
# 

echo  " -------------------------------------------------"
echo  "check out proteinsYYMMDD tables :  hugo, etc.  see push protocols"
echo  "        set gdbPdb after tables pushed           "
echo  " -------------------------------------------------"
echo
# 
echo
echo " -------------------------------------------------"
echo "    When the gene set is stable, notify  Tim Strom (TimStrom@gsf.de)"
echo "    so he can change his ExonPrimer - if a human or mouse assembly"
echo " -------------------------------------------------"
echo

echo
echo " -------------------------------------------------"
echo "    Once the new UCSC Genes is released on RR, "
echo "         update .../htdocs/knownGeneLists.html"
echo " -------------------------------------------------"
echo "\n here are files for /usr/local/apache/htdocs/knownGeneList/$db"
echo
ls -l /usr/local/apache/htdocs/knownGeneList/$db
echo

echo
echo " -------------------------------------------------"
echo "            now go do Family Browser" 
echo " -------------------------------------------------"
echo
echo "  then run bigPush.csh ??"

# clean up unneeded files:
rm -f *desc
rm -f *Cols
rm -f $db.chr*

# "clear out empty files:
~/bin/dumpEmpty.csh . >& /dev/null

echo
echo end
echo

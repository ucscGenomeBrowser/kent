#!/bin/tcsh

# to do:  check that trackDb.hgGene is "on"
# to do:  see that cgapBiocDesc has unique entries

###############################################
# 
#  03-09-04
#  Robert Kuhn
#
#  Runs through the checks on Known Genes.  
#  Requires KG java run in Trackchecking first.
#  -----------
#  run KGGeneCheck.java on dev: (need to run HGGeneCheck now -- in CVS)
#  /cluster/home/heather/TrackChecking/KGGeneCheck.java
# 
###############################################

set db=""
set oldDb=""
set trackName="kg"


if ($#argv == 0 || $#argv > 2) then
  # no command line args
  echo
  echo "  runs test suite on Known Genes track."
  echo "  expects files, {kg,fb,pb}Tables, up one directory."
  echo
  echo "    usage:  database, [oldDb] (if fresh assembly for existing organism)"
  echo
  exit
else
  set db=$argv[1]
endif

if ($#argv == 2) then
  set oldDb=$argv[2]
else
  set oldDb=$db
endif


echo "using $db "
echo "using $oldDb for beta db "
set curr_dir=$cwd


# --------------------------------------------
#  get sets of KG, FB(GS), PB tables:
# also prints list and update times.

/cluster/home/kuhn/bin/findKgTableSet.csh $db 

echo
echo "the following tables are in the overall KG list, but not in this assembly:"
comm -23 kgTables kgTablesAll
echo  

echo
echo "the following tables are in the overall GS list, but not in this assembly:"
comm -23 fbTables fbTablesAll
echo  

echo
echo "the following tables are in the overall PB list, but not in this assembly:"
comm -23 pbTables pbTablesAll
echo  

# --------------------------------------------
#  run KGGeneCheck.java on dev:

# cd /cluster/home/heather/TrackChecking/
# source javaenv
# java KGGeneCheck $db > $db.$trackName &
# cd curr_dir

# --------------------------------------------
# with all new tables on dev, check their update times relative to beta:

# now done with kgFindTableSet.csh above

# echo
# echo "update times ($trackName):"
# echo
# rm -f differentTablesAll
#   echo "Table \n============= \n"."dev \n"."beta \n"
#   echo
# foreach table (`cat {$trackName}TablesAll`)
#   echo $table
#   echo "============="
#   set dev=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db | awk '{print $13, $14}'`
#   set beta=`hgsql -h hgwbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db | awk '{print $13, $14}'`
#   echo "."$dev
#   echo "."$beta
# 
#   if ("$dev" != "$beta") then
#     echo $table >> differentTablesAll
#     echo $table " does not match"
#   endif
#   echo
# end
# echo


echo
echo "compare new genes to old set (uniq):"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
hgsql -N -e "SELECT name from knownGene" $db | sort | uniq > $db.KG.name.dev
hgsql -N -h hgwbeta -e "SELECT name from knownGene" $oldDb | sort |uniq > $oldDb.KG.name.beta.uniq
comm -23 $db.KG.name.dev $oldDb.KG.name.beta.uniq > $db.KG.name.devOnly
comm -13 $db.KG.name.dev $oldDb.KG.name.beta.uniq > $oldDb.KG.name.betaOnly
comm -12 $db.KG.name.dev $oldDb.KG.name.beta.uniq > $db.KG.name.commonOnly
wc -l *Only | grep -v total
echo
echo "check some new ones by hand:"
head -20 $db.KG.name.devOnly | tail -3
echo
echo "check some old ones by hand on beta:"
head -20 $oldDb.KG.name.betaOnly | tail -3
echo

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

# echo
# echo "check if browser show no 5', 3'-UTR where predicted:"
# echo "no 5'-UTR:"
# hgsql e "SELECT name, chrom, txStart, txEnd FROM knownGene \
#          WHERE txStart = cdsStart LIMIT 3" $db
# echo
# echo "no 3'-UTR:"
# hgsql e "SELECT name, chrom, txStart, txEnd FROM knownGene \
#          WHERE txEnd = cdsEnd LIMIT 3" $db
# echo:


# -------------------------------------------------
# check ends:

# looking for off the end of each chrom (look for all positive values)
# looking for off-end of each chrom (look for all positive values)

echo
echo "looking for off the end of each chrom \
       (look for all positive values):"

# echo "transcription coords"
# echo "printing tx to file"
# print tx to file:
hgsql -e "SELECT chromInfo.chrom, chromInfo.size - MAX(knownGene.txEnd) \
    AS dist_from_end FROM chromInfo, knownGene \
    WHERE chromInfo.chrom = knownGene.chrom \
    GROUP BY chromInfo.chrom" $db > $db.KG.tx.offEnd
# echo


echo "lines from $db.KG.tx.offEnd > 0:"
awk '{if($2<0) {print $2} }' $db.KG.tx.offEnd
echo "expect blank or check file $db.KG.tx.offEnd"
echo



# -------------------------------------------------
# check to see if there are genes on all chroms.


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


# -------------------------------------------------
# check exon sizes:


# edit the list /cluster/home/heather/jdbc/data/tablelist
echo
echo "knownGene" > /cluster/home/heather/jdbc/data/KG

# and run /cluster/home/heather/jdbc/ExonSize.java:
cd /cluster/home/heather/jdbc/
source javaenv
java ExonSize $db data/KG | egrep -v "items found|-----|Querying" > $curr_dir/$db.exonExtremes
cd $curr_dir 

echo
echo "number of very large or negative exons:"
grep "exon" $db.exonExtremes | wc -l
# echo "five lines are kruft (filtered above now with grep -v"
echo
echo "example of very large or negative exons:"
head -5 $db.exonExtremes
echo


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



# delete lines with single hit "   1[tab]" 
# -- and count lines remaining (genes used more than once):

echo
sort  $db.KG.name.sort | uniq -c | sort -nr > $db.KG.name.uniq.most

echo "number of genes that align more than once:"
sed -e "/  1 /d" $db.KG.name.uniq.most | wc -l
echo

echo "gene occurance profile"
echo
textHistogram -col=1 $db.KG.name.uniq.most
rm file

echo
echo "genes that align the most times:"
head $db.KG.name.uniq.most
echo



# -------------------------------------------------
# examine overlapping genes ???
#   .../jdbc/Overlap.java
#   how ???  There are many
# -------------------------------------------------
# look for same number of rows in kgXref, knownGenePep, knownGeneMrna

echo
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
echo "should match uniqs in KG:"
wc -l $db.KG.name.uniq
echo

echo "dupes in kgXref.kgID field:"
if ($xrefNum != $kgPep) then
  hgsql -e "SELECT kgID, COUNT(*) as number FROM kgXref GROUP BY kgID \
    ORDER BY number DESC LIMIT 5 " $db
endif

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



echo "check that names in knownGene match names in knownGenePep, knownGeneMrna:"
diff $db.KG.name.uniq $db.KGpep.name.uniq | wc -l
diff $db.KG.name.uniq $db.KGrna.name.uniq | wc -l
echo "expect zero"
echo

echo "check that names in knownGene match kgID in kgXref and kgProtAlias:"
# diff $db.KG.name.uniq $db.kgXref.kgID.uniq | wc -l
# diff $db.KG.name.uniq $db.kgProtAlias.kgID.uniq | wc -l

echo "uniq to KG.name (not in kgXref.kgID):"
comm -23 $db.KG.name.uniq $db.kgXref.kgID.uniq | wc -l
echo "uniq to kgXref.kgID: (not in KG.name):"
comm -13 $db.KG.name.uniq $db.kgXref.kgID.uniq | wc -l

echo "uniq to KG.name (not in kgProtAlis.kgID):"
comm -23 $db.KG.name.uniq $db.kgProtAlias.kgID.uniq | wc -l
echo "uniq to kgProtAlias.kgID: (not in KG.name):"
comm -13 $db.KG.name.uniq $db.kgProtAlias.kgID.uniq | wc -l

echo "expect zero"
echo "as of KGII, duplicate kgXref.kgID are ok.  will be corrected in KGIII."
echo

# -------------------------------------------------
# knownGenePep, knownGeneMrna -- check numbers of entries vs old version
# knownGenePep, knownGeneMrna -- check min and max lengths of seq column

echo
echo "knownGenePep, knownGeneMrna -- check numbers of entries vs old version:"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
echo "dev first"
echo
echo "Pep"
hgsql -N -e "SELECT COUNT(*) FROM knownGenePep" $db
hgsql -h hgwbeta -N -e "SELECT COUNT(*) FROM knownGenePep" $oldDb
echo
echo "Mrna"
hgsql -N -e "SELECT COUNT(*) FROM knownGeneMrna" $db
hgsql -h hgwbeta -N -e "SELECT COUNT(*) FROM knownGeneMrna" $oldDb
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
 hgsql -h hgwbeta -N -e "SELECT MIN(LENGTH(seq)) FROM knownGeneMrna" $oldDb
 hgsql -h hgwbeta -N -e "SELECT MAX(LENGTH(seq)) FROM knownGeneMrna" $oldDb
 hgsql -h hgwbeta -N -e "SELECT MIN(LENGTH(seq)) FROM knownGenePep" $oldDb
 hgsql -h hgwbeta -N -e "SELECT MAX(LENGTH(seq)) FROM knownGenePep" $oldDb
echo "sometimes the same on dev as beta:  no new proteins at extremes "
echo

# -------------------------------------------------
# spMrna -- check for unique spID 

echo
echo "spMrna -- check for unique spID:"
set idnum=`hgsql -N -e "SELECT spID FROM spMrna" $db | sort -u | wc -l`
set idtot=`hgsql -N -e "SELECT COUNT(*) FROM spMrna" $db`
set diff = `expr $idtot - $idnum`
echo "  dupes = $diff"
echo "dupes ok as of KG2."
echo

# -------------------------------------------------
# spMrna -- no mrnaID like NM%

echo
echo "spMrna -- now includes this many NM_ in spMrna.mrnaID field"
hgsql -e 'SELECT COUNT(*) FROM spMrna WHERE mrnaID LIKE "NM%"' $db
echo


# -------------------------------------------------
# spMrna --  can search on spMrna.spID
# -- make a java robot for this (uses hgGene).

echo
echo "spMrna --  can search on spMrna.spID"
hgsql -e "SELECT spID FROM spMrna limit 5" $db
echo
echo "manual check:  type some into Position box"
echo

# -------------------------------------------------
# dupSpMrna -- all values of dupSpMrna.mrnaID should also be in spMrna.mrnaID 
# make sorted file of dupSpMrna.mrnaID 
# make sorted file of spMrna.mrnaID 
# diff 'em
# look for all diffs to be in one direction:


echo
echo "dupSpMrna -- all values of dupSpMrna.mrnaID used to be in spMrna.mrnaID."
echo "             reporting the number of dupSpMrna.mrnaID not in spMrna.mrnaID."
hgsql -N -e "SELECT mrnaID FROM dupSpMrna" $db | sort -u > $db.dupSp.uniq
hgsql -N -e "SELECT mrnaID FROM spMrna" $db | sort -u > $db.spMrna.uniq
comm -23 $db.dupSp.uniq $db.spMrna.uniq > $db.sp.diff
set dupCheck=`wc -l $db.sp.diff | awk '{print $1}'`
set spTot=`wc -l $db.dupSp.uniq | awk '{print $1}'`
if ($dupCheck == 0) then
  echo "  all dupSpMrna.mrnaID are in spMrna.mrnaID."
else
  echo
  echo "  $dupCheck dupSpMrna.mrnaID out of $spTot are not in spMrna.mrnaID."
  echo "  see $db.sp.diff for list.  sample: \n"
  head $db.sp.diff
endif
echo

echo "find out if any in diff list are in =other= column of spMrna "
hgsql -N -e "SELECT spID FROM spMrna" $db | sort -u > $db.spMrna.spID.uniq
comm -23 $db.sp.diff $db.spMrna.spID.uniq > $db.spMrna.leftOvers2
echo "this many are not:"
wc -l *leftOvers2
echo "expect zero"


# -------------------------------------------------
# kgXref -- contains rows from knownGeneLink.  
# These have kgXref.kgID= kgXref.refseq
# compare to knownGeneLink.name

echo
echo "kgXref -- contains rows from knownGeneLink."
echo "  These have kgXref.kgID = kgXref.refseq"
hgsql -N -e "SELECT COUNT(*) FROM kgXref WHERE kgXref.kgID = kgXref.refseq" $db
hgsql -N -e "SELECT kgID FROM kgXref WHERE kgID = refseq" $db | sort | uniq > $db.kgXref.maybeLink
hgsql -N -e "SELECT name FROM knownGeneLink" $db > $db.knownGeneLink.name
comm -12 $db.kgXref.maybeLink  $db.knownGeneLink.name | wc -l
echo "expect knownGeneLink to be empty since KG2 "
echo "   -- DNA-based refSeqs are already in the build."
echo "   Push empty table to keep code happy."
echo "   didn't do it for hg18 and no evidence of problems." 
echo "   will also not do it for mm8 and see."
echo


# -------------------------------------------------
# kgXref -- mrna generally equals kgID

echo
echo "kgXref -- mrna generally equals kgID"
echo "count:"
hgsql -N -e "SELECT COUNT(*) FROM kgXref" $db
echo "count mrna != kgID:"
hgsql -N -e "SELECT COUNT(*) FROM kgXref WHERE mrna != kgID" $db
echo "expect zero"
echo


# -------------------------------------------------
# kgXref -- empty fields
# should give zero (no empty fields):


echo
echo "refseq column can be empty (protAcc is, too, then)"
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE refseq = "" ' $db
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE refseq = "" AND protAcc = "" ' $db
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE refseq = "" AND protAcc != "" ' $db
echo "expect last value to be zero"
echo


# check all tables for null + zero entries:

echo
echo "check all tables for null + zero entries:"
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
echo "some, but not all  of protAcc column will be empty:"
echo "kgXref.protAcc empty:"
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE protAcc = "" ' $db
echo "kgXref.protAcc not empty:"
hgsql -N -e 'SELECT COUNT(*) FROM kgXref WHERE protAcc != "" ' $db
echo


# -------------------------------------------------
# kgXref -- spID are generally equal to spDisplayID

echo
echo "kgXref -- spID are equal to spDisplayID if protein is in TrEMBL"
echo "match:"
hgsql -N -e "SELECT COUNT(*) FROM kgXref WHERE spID = spDisplayID" $db
echo "do not match:"
hgsql -N -e "SELECT COUNT(*) FROM kgXref WHERE spID != spDisplayID" $db
echo


# -------------------------------------------------
# kgXref -- can have duplicate values in spID


echo
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

echo "kgXref -- can position-search on spID:"
echo "check manually"
hgsql -N -e "SELECT spID FROM kgXref ORDER BY spID DESC limit 2" $db
echo


# -------------------------------------------------
# kgAlias -- check kgAlias.alias for uniq.  search position box with some

echo
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

echo
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
echo "kgAlias -- expect multiple rows with same kgID value"
set tot=`hgsql -N -e "SELECT COUNT(*) FROM kgAlias" $db`
set uniq=`hgsql -N -e "SELECT kgID FROM kgAlias" $db | sort | uniq | wc -l`
set diff = `expr $tot - $uniq`
echo "tot   = "$tot
echo "uniq  = "$uniq
echo "dupes = "$diff
echo

# -------------------------------------------------
# mrnaRefseq --  table has only two columns, mrna and refesq.
#              check uniq


echo
echo "mrnaRefseq --  table has only two columns, mrna and refesq:"
echo "total mrnaRefseq:"
hgsql -N -e "SELECT COUNT(*) FROM mrnaRefseq" $db
echo "total uniq mrnaRefseq.mrna:"
hgsql -N -e "SELECT mrna FROM mrnaRefseq" $db | sort -n | uniq |  wc -l 
echo "total uniq mrnaRefseq.refseq:"
hgsql -N -e "SELECT refseq FROM mrnaRefseq" $db | sort -n | uniq | wc -l
echo

# -------------------------------------------------
# mrnaRefseq --  generate list of uniq mrnaAcc values in refGene that are
#                not in mrnaRefseq. These are "DNA-based"


echo
echo "FORMERLY: check that all DNA-based genes are in refGene.name"
echo "NOW: find refGenes that have been updated since mrnRefseq was built."
echo "(KG no longer has a separate DNA leg or knownGeneLink table.
echo "  -- DNA-based refseqs are already in the build)"
hgsql -N -e "SELECT name FROM refGene" $db | sort | uniq \
   > $db.refGene.name.uniq 
wc -l $db.refGene.name.uniq
hgsql -N -e "SELECT refseq FROM mrnaRefseq" $db | sort | uniq \
   > $db.mrnaRefseq.refseq.uniq
echo "refGene.name and mrnaRefseq.refseq have in common:"
comm -12 $db.refGene.name.uniq  $db.mrnaRefseq.refseq.uniq | wc -l 

comm -23 $db.refGene.name.uniq  $db.mrnaRefseq.refseq.uniq \
   > refGene.notin.mrnaRefseq
echo 'refGene.name not in mrnaRefseq.refseq (new refseq since KG build):'
comm -23 $db.refGene.name.uniq  $db.mrnaRefseq.refseq.uniq | wc -l

echo "this value should be small"
echo "  -- check some:"
comm -23 $db.refGene.name.uniq  $db.mrnaRefseq.refseq.uniq | head -3 
echo


# -------------------------------------------------
# cgapAlias -- check that at least some cgapAlias.alias 
#    entries are in knownGene.name

echo
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
echo "compare description of all tables with previous"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
foreach table (`cat kgTablesAll`)
  hgsql -h hgwbeta -N  -e "DESCRIBE $table" $oldDb >  $db.beta.$table.desc
  hgsql -N  -e "DESCRIBE $table" $db > $db.dev.$table.desc
  echo $table
  diff $db.dev.$table.desc $db.beta.$table.desc
  echo
end
echo

# ------------
# "check for one missing knownGeneLink:"
# deprecated.  knownGeneLlink is now empty but pushed.

# echo
# echo "check for one missing knownGeneLink:"
# hgsql -N -e "SELECT name FROM knownGeneLink" $db \
#    | sort > $db.dev.kgLink.sort
# hgsql -h hgwbeta -N -e "SELECT name FROM knownGeneLink" $db \
#    | sort > $db.beta.kgLink.sort
# echo "count and display records deleted from old version"
# comm -13 $db.dev.kgLink.sort $db.beta.kgLink.sort | wc -l
# comm -13 $db.dev.kgLink.sort $db.beta.kgLink.sort | head -3
# echo "some will be found anyway because they are in mrnaRefseq.kgID"
# echo "   but the mRNA on the details page will not be the same."
# echo

# find a way to count "<" and ">" lines in a file  
# "sed effort below didn't work

# hgsql -N -e 'SELECT name FROM knownGeneLink WHERE name = "NM_198864"' $db 
# found: therefore "<" in a diff means it is in the first file named
# hgsql -h hgwbeta -N -e 'SELECT name FROM knownGeneLink \
#       WHERE name = "NM_198864"' $db 
# not found: therefore "<" in a diff means it is in the first file named

###############################
# didn't work:
# diff $db.dev.kgLink.sort $db.beta.kgLink.sort | sed -e '/>/' 
###############################
# didn't work:


echo
echo "check rowcounts against beta"
echo "old version, beta,  listed first"
if ($db != $oldDb) then
  echo "  comparing to table in $oldDb on beta"
  echo
endif
foreach table (`cat kgTablesAll`)
  set old=`hgsql -h hgwbeta -N  -e "SELECT COUNT(*) FROM $table" $oldDb`
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
hgsql -h hgwbeta -N -e "SELECT * FROM kgAlias" $oldDb | sort > $oldDb.beta.kgAlias.sort
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
echo "    notify  Tim Strom (TimStrom@gsf.de)"
echo "    so he can change his ExonPrimer - if a human or mouse assembly"
echo " -------------------------------------------------"
echo

echo
echo " -------------------------------------------------"
echo "    check to see if .../htdocs/knownGeneLists.html has been updated"
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

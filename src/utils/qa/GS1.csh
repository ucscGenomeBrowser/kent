#!/bin/tcsh
source `which qaConfig.csh`

############################
#  03-11-04
#  last updated
#  04-09-04
# 
#  added command line db
# 
############################

set newRows=""
set db=""
set betaDb=""

if ($#argv < 1 ||  $#argv > 2 ) then
  echo
  echo "  runs Gene Sort testing"
  echo "  list of tables, fbTablesAll, is expected up one directory"
  echo
  echo "    usage: database, [oldDb]"
  echo "      where oldDb is name of previous aseembly on beta."
  echo
  exit
else
  set db=$argv[1]
endif
set url1="http://hgwdev.soe.ucsc.edu/cgi-bin/hgTracks?db=$db"

if ($#argv == 2 ) then
  set betaDb=$argv[2]
else
  set betaDb=$db
endif

echo $db $betaDb

if (-e ../fbTablesAll)
  cp ../fbTablesAll .
  set tablelist="fbTablesAll"
else
  echo
  echo " sorry, need file:  ../fbTablesAll "
  echo " this is the file of tables specific to this assembly's GS"
  echo
  exit
endif

echo "using:"
echo "db = $db, betaDb = $betaDb"
echo "tablelist=$tablelist"
echo

# -------------------------------------------------
# check update times against beta:


echo "check update times against beta:"
if ( $betaDb != "" ) then
  echo "\n note: time comparison against other db is irrelevant.|n"
endif

updateTimes.csh $db $tablelist 


# -------------------------------------------------
# check rowcounts against beta:

echo
echo "check rowcounts against beta"
echo "old version, beta ($betaDb),  listed first"
foreach table (`cat $tablelist`)
  echo
  echo $table
  echo "================="
  rm -f junk 
  set old=`hgsql -h $sqlbeta -N  -e "SELECT COUNT(*) FROM $table" $betaDb`
  set new=`hgsql -N  -e "SELECT COUNT(*) FROM $table" $db`
  if ($old != "") then
    set newRows=`expr $new - $old`
    set percent=`echo $newRows $old | awk '{printf "%.2f\n", ($1/$2)*100}'`
  else
    set percent="no old table"
  endif
  echo "   old:  $old"
  echo "   new:  $new"
  echo "   more: $newRows, ($percent)%"
end
echo


# -------------------------------------------------
# compare description of all tables with previous:

echo 
echo "compare description of all tables with previous (shows diffs):"
foreach table (`cat $tablelist`)
  echo $table
  hgsql -h $sqlbeta -N  -e "DESCRIBE $table" $betaDb >  $betaDb.beta.$table.desc
  hgsql -N  -e "DESCRIBE $table" $db > $db.dev.$table.desc
  diff $betaDb.beta.$table.desc $db.dev.$table.desc
  echo
end
echo

# -------------------------------------------------
# SELECT one record FROM knownCanonical:
# SELECT one record FROM knownIsoforms:

echo
echo "SELECT two records FROM knownCanonical:"
echo "================="
hgsql -t -e "SELECT * FROM knownCanonical limit 2" $db
echo

echo
echo "SELECT two records FROM knownIsoforms:"
echo "================="
hgsql -t -e "SELECT * FROM knownIsoforms limit 2" $db
echo

# -------------------------------------------------
# check that all knownGenes are in knownIsoforms:

echo
echo "check that all knownGenes are in knownIsoforms:"
hgsql -N -e "SELECT name FROM knownGene" $db > $db.KG.name
sort $db.KG.name | uniq > $db.KG.name.uniq
hgsql -N -e "SELECT name FROM knownGene" $db | sort | uniq > $db.KG.name.uniq
hgsql -N -e "SELECT transcript FROM knownCanonical" $db > $db.knCanonical.transcript
hgsql -N -e "SELECT transcript FROM knownIsoforms" $db  > $db.knIsoforms.transcript
sort $db.knCanonical.transcript | uniq > $db.knCanonical.transcript.uniq
sort $db.knIsoforms.transcript  | uniq > $db.knIsoforms.transcript.uniq
wc -l $db.knCanonical.transcript.uniq 
wc -l $db.knIsoforms.transcript.uniq 
wc -l $db.KG.name.uniq 
echo

# -------------------------------------------------
# names in common between KG.name and knIsoforms.transcript (expect all):

echo
echo  "names in common between KG.name and knIsoforms.transcript (expect all):"
comm -12 $db.KG.name.uniq $db.knIsoforms.transcript.uniq  | wc -l
echo


# -------------------------------------------------
# transcripts in common between knIsoforms and knCanonical

echo
echo "transcripts in common between knIsoforms and knCanonical"
echo "   (expect one list to be found entirely within other):"
comm -12 $db.knCanonical.transcript.uniq $db.knIsoforms.transcript.uniq  | wc -l
echo

# -------------------------------------------------
# check that there are more in new release:

echo
echo
echo "check that there are more in new release:"
hgsql -h $sqlbeta -N -e "SELECT name FROM knownGene" $betaDb \
  > beta.$betaDb.KG.name
sort beta.$betaDb.KG.name | uniq > beta.$betaDb.KG.name.uniq
hgsql -h $sqlbeta -N -e "SELECT transcript FROM knownCanonical" $betaDb \
  > beta.$betaDb.knCanonical.transcript
hgsql -h $sqlbeta -N -e "SELECT transcript FROM knownIsoforms" $betaDb \
  > beta.$betaDb.knIsoforms.transcript
sort beta.$betaDb.knCanonical.transcript | uniq \
  > beta.$betaDb.knCanonical.transcript.uniq
sort beta.$betaDb.knIsoforms.transcript  | uniq \
  > beta.$betaDb.knIsoforms.transcript.uniq
echo
wc -l *transcript* | grep -v total
echo
wc -l *KG* | grep -v total
echo



echo "-------------------------------------------------"
# make list of dupes:

echo
echo "make list of dupes"
hgsql -e "SELECT clusterID from knownIsoforms" $db | sort | uniq -c \
   | sort -nr > $db.knIsoforms.clusterID.sort
# delete those found only once:
sed -e "/    1\t/d" $db.knIsoforms.clusterID.sort \
   > $db.knIsoforms.clusterID.dupes
echo "number of duplicated knownIsoforms.clusterID:"
wc -l $db.knIsoforms.clusterID.dupes
echo
echo "most common duplicated knownIsoforms.clusterID:"
head $db.knIsoforms.clusterID.dupes 
echo
# make links for the three biggest:
echo "see $db.knIsoforms.mostCommon and check that each \
         clusterID refers to a set of overlapping transcripts \
         in the Browser:"
foreach cluster ( `head -3 $db.knIsoforms.clusterID.dupes | awk '{print $2}'` )
  set pos=`hgsql -Ne 'SELECT chrom, chromStart, chromEnd FROM knownCanonical \
    WHERE clusterId = "'$cluster'"' $db | sed "s/\t/:/" | sed "s/\t/-/"`
  echo "$url1&position=$pos"
end

echo
echo "check these coordinate blocks manually in GB and click-through to GS."
echo "the number of transcripts in the window might not match the number \
     expected because the canonical may be smaller than one of the isoforms. \
     zoom out a little to get them all"
echo

echo "-------------------------------------------------"
# check for dupes in knownIsoforms

echo
echo check for dupes in knownIsoforms
hgsql -N -e "SELECT * FROM knownIsoforms" $db | sort > $db.knownIsoforms.record
uniq $db.knownIsoforms.record > $db.knownIsoforms.record.uniq
wc -l *record* | grep -v total
echo "difference is number of dupes"
echo "  dupes:"
comm -23 $db.knownIsoforms.record  $db.knownIsoforms.record.uniq \
  > $db.knownIsoforms.record.dupes
head $db.knownIsoforms.record.dupes
echo

 
# -------------------------------------------------
# checking off chrom end:

echo
echo "checking off chrom end:"
set table=knownCanonical
# hgsql -e "SELECT chromInfo.chrom, chromInfo.size - MAX($table.chromEnd) AS dist_from_end FROM chromInfo, $table WHERE chromInfo.chrom = $table.chrom GROUP BY chromInfo.chrom" $db | awk '{if ($2<0) print $1 $2; else print $1, "ok"}'

hgsql -e "SELECT chromInfo.chrom, chromInfo.size - MAX($table.chromEnd) AS dist_from_end FROM chromInfo, $table WHERE chromInfo.chrom = $table.chrom GROUP BY chromInfo.chrom" $db >  $db.GS.tx.offEnd

echo "lines from $db.GS.tx.offEnd > 0:"
awk '{if($2<0) {print $2} }' $db.GS.tx.offEnd
echo "expect blank or check file $db.GS.tx.offEnd"
echo

# -------------------------------------------------
# checking start < end and start < 0:

echo
echo "checking start < end and start < 0:"
hgsql -N -e "SELECT * FROM $table WHERE chromStart >= chromEnd" $db
hgsql -N -e "SELECT * FROM $table WHERE chromStart < 0" $db
echo "expect nothing"
echo


# -------------------------------------------------
# show indices:

# echo
# echo "show indices:"
# foreach table (`cat $tablelist`)
#   hgsql -t -e "SHOW INDEX FROM $table" $db
#   echo
# end
# echo 


echo "-------------------------------------------------"
# get one record from each table:

echo "get one record from each table:"
echo


cat $tablelist | grep knownTo > $db.knownTo
foreach table (`cat $db.knownTo`)
  echo $table
  echo "============="
  echo "one record:"
  hgsql -t -e "SELECT * FROM $table LIMIT 1" $db
  if ($table == "knownToSuper") then
  # ?? they are the same -- where were you going with this?
    set old=`hgsql -h $sqlbeta -N -e "SELECT gene FROM $table" $betaDb \
        | sort | uniq | wc -l`
    set this=`hgsql -N -e "SELECT gene FROM $table" $db | sort | uniq | wc -l`
  else
    set old=`hgsql -h $sqlbeta -N -e "SELECT name FROM $table" $betaDb \
        | sort | uniq | wc -l`
    set this=`hgsql -N -e "SELECT name FROM $table" $db | sort | uniq | wc -l`
  endif
  set kgNames=`hgsql -N -e  'SELECT name FROM knownGene' $db \
      | sort | uniq | wc -l`
  echo "number of uniq names in the tables"
  echo "beta       = "$old
  echo "this table = "$this
  echo "knownGene  = "$kgNames
  echo
end


echo " -------------------------------------------------"
echo "            now run GS2.csh           "
echo " -------------------------------------------------"
echo

echo " -------------------------------------------------"
echo "            don't forget to change gdbPdb entry for this \
                 assembly as tables are pushed to beta and RR."
echo " -------------------------------------------------"
echo

echo "end"
rm -f *desc


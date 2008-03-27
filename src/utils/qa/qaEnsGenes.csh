#!/bin/tcsh

###############################################
# 
#  03-18-08
#  Ann Zweig
#
#  Runs through the usual checks for Ensembl
#  Gene updates.
# 
###############################################

set runOn=''
set ver=0
set dbs=''
set db=''

if ($#argv != 2 ) then
  echo
  echo "  runs test suite for ensGene track update"
  echo "  run this script before you push the new tables to beta"
  echo "  (makes lots of files: run in junk directory)"
  echo "  (it's best to direct output and errors to a file: '>&')"
  echo
  echo "    usage: ensGeneVersionNumber (db | all)"
  echo "      ensGeneVersionNumber  = Ensembl's version, e.g. 49"
  echo "      choose one database (db) or all dbs with ensGenes tracks (all)"
  echo
  exit 1
else
  set ver=$argv[1]
  set runOn=$argv[2]
endif

# run only from hgwdev
if ( "$HOST" != "hgwdev" ) then
  echo "\nERROR: you must run this script on hgwdev!\n"
  exit 1
endif

# check input
if ( $ver <= 45 || $ver >= 100 ) then
  echo "\nERROR: you must enter a valid version number!\n"
  exit 1
endif

if ( -e xxDbListxx) then
 rm xxDbListxx
endif

# figure out which databases we're running it on
if ( 'all' == $runOn ) then
 set dbs=`hgsql -Ne "SELECT db FROM trackVersion WHERE version = '$ver' \
 ORDER BY db" hgFixed | sort -u` 
 if ( "" == "$dbs" ) then
  echo "\nERROR: there is no update available for version number $ver\n"
  exit 1
 else
  echo "\nRunning script for Ensembl Genes v$ver on these assemblies:\n"
  foreach db ($dbs)
   set onBeta=`hgsql -h hgwbeta -Ne "SELECT name FROM dbDb \
   WHERE name = '$db' AND active = 1" hgcentralbeta`
   if ( '' == $onBeta ) then
    echo "$db (ensGenes have been updated on dev, but this database is not \
         active on hgwbeta: skipping)"
   else
    echo $db
    echo $db >> xxDbListxx
   endif
  end
  set dbs=`cat xxDbListxx`
 endif
else
 echo "\nRunning script for Ensebml Genes v$ver on this assembly:\n"
 set dbs=$runOn
 echo $dbs
endif

echo "\n\n----------------------"
echo "compare new (dev) and old (beta) ens* tables"
echo "this shows the counts of rows unique to dev, unique to beta, and"
echo "present on both.  you should be suspicious if there are big differences"
foreach db ($dbs)
 compareWholeColumn.csh $db ensGene name
 compareWholeColumn.csh $db ensPep name
 compareWholeColumn.csh $db ensGtp transcript
 echo
end

echo "\n\n----------------------"
echo "check a few of the new additions to the ensGene table"
echo "(be sure to click all the way out to Ensembl website)"
foreach db ($dbs)
 echo "\ncheck these in $db browser on hgwdev:"
 head -2 $db.ensGene.name.devOnly
 tail -2 $db.ensGene.name.devOnly
end

echo "\n\n----------------------"
echo "check a few of the deleted items from the ensGene table"
echo "(make sure they have also been dropped from Ensembl website)"
foreach db ($dbs)
 echo "\ncheck these in $db browser on hgwbeta:"
 head -2 $db.ensGene.name.betaOnly 
 tail -2 $db.ensGene.name.betaOnly
end

echo "\n\n----------------------"
echo "these are full sets of corresponding rows from the three tables:"
echo "ensGene <-> ensPep <-> ensGtp on hgwdev"
foreach db ($dbs)
 echo "\ncheck these two genes (and their peptides) on hgwdev for '$db':"
 hgsql -e "SELECT * FROM ensGene, ensPep, ensGtp WHERE \
 ensGene.name = ensPep.name AND ensGene.name = ensGtp.transcript LIMIT 2\G" $db
end

echo "\n\n----------------------"
echo "run genePredCheck on the ensGene table. if there a failure here,"
echo "then something is seriously wrong with the ensGene table."  
echo "MarkD can help you figure out exactly what's wrong."
foreach db ($dbs)
 echo "\ngenePredCheck results for $db.ensGene on hgwdev:" 
 genePredCheck -db=$db ensGene
end

echo "\n\n----------------------"
echo "find out which chroms the genes are on (for both dev and beta).  
echo "look for unusually small or large numbers here (or big differences)."
foreach db ($dbs)
 # don't run this on scaffold assemblies
 set numChroms=`hgsql -Ne "SELECT COUNT(*) FROM chromInfo" $db`
 if ( $numChroms < 100 ) then
  countPerChrom.csh $db ensGene $db hgwbeta
 else
  echo "$db is a scaffold assembly: skipping countPerChrom"
 endif
 echo
end

echo "\n\n----------------------"
echo "featureBits for new (dev) and old (beta) tables"
foreach db ($dbs)
 echo "\nfeatureBits $db ensGene (on hgwdev):"
 featureBits $db ensGene
 echo "featureBits $db -countGaps ensGene (on hgwdev):"
 featureBits $db -countGaps ensGene
 echo "featureBits $db -countGaps ensGene gap (on hgwdev):"
 featureBits $db -countGaps ensGene gap
 echo "\nfeatureBits $db ensGene (on hgwbeta):"
 ssh hgwbeta featureBits $db ensGene
 echo "featureBits $db -countGaps ensGene (on hgwbeta):"
 ssh hgwbeta featureBits $db -countGaps ensGene
 echo "featureBits $db -countGaps ensGene gap (on hgwbeta):"
 ssh hgwbeta featureBits $db -countGaps ensGene gap
 echo
end

echo "\n\n----------------------"
echo "run Joiner Check. look for errors in the following two lines only:"
echo "ensPep.name and ensGtp.transcript"
foreach db ($dbs)
 echo "\nrunning joinerCheck for $db on ensemblTranscriptId:"
 joinerCheck -keys -database=$db -identifier=ensemblTranscriptId ~/kent/src/hg/makeDb/schema/all.joiner
end

echo "\n\n----------------------"
echo "ensGene names typically begin with 'ENS'. if there is a number other"
echo "than 0, then there are ensGenes that do not begin with 'ENS'."
echo "check them out on the Ensembl website."
foreach db ($dbs)
 echo "\nnumber of ensGenes that do not begin with 'ENS' in '$db':"
 set num=`hgsql -Ne "SELECT COUNT(*) FROM ensGene WHERE name \
 NOT LIKE 'ENS%'" $db`
 echo $num
 if ( 0 != $num ) then
  echo "instead of 'ENS', the ensGenes in this table look like this:"
  hgsql -Ne "SELECT name FROM ensGene WHERE name NOT LIKE 'ENS%' LIMIT 3" $db
 endif
end

# clean up
if ( -e xxDbListxx) then
 rm xxDbListxx
endif

echo "\nthe end.\n"

exit 0

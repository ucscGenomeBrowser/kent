#!/bin/tcsh -e
# BUILD $db OMIM RELATED TRACKS

set db=$1

# Build genemap.tab.  Basically, insert additional tabs to pad out fields that used
# to be split into multiple fields. (e.g., comments used to be comments1 and comments2)
# This works, but we've lost the reference column info entirely.  Not available from OMIM.
# Did we ever make use of that anyway?
grep -v '^#' genemap.txt | awk 'BEGIN {FS="\t"; OFS="\t"};
   {$8 = $8 OFS; $11 = $11 OFS; $12 = $12 OFS OFS; $13 = $13 OFS; print $0}' > genemap.tab

hgLoadSqlTab -verbose=0 -warn $db omimGeneMapNew ~/kent/src/hg/lib/omimGeneMap.sql genemap.tab 

# Load mim2gene table

grep -v '^#' mim2gene.txt | awk 'BEGIN {FS="\t"; OFS="\t"}; $2 ~ "gene/phenotype"
  {$2 = "gene"; print $0; $2 = "phenotype"}; {print $0}' > mim2gene.updated.txt

hgLoadSqlTab -verbose=0 -warn $db omim2geneNew ~/kent/src/hg/lib/omim2gene.sql mim2gene.updated.txt

# Not sure what this file is created for.  Can probably remove this?
awk 'BEGIN {FS="\t"; OFS="\t"}; {print $1, $3, $2}' mim2gene.updated.txt > mim2gene.tab


############################################################
# build omimGeneSymbol and omimGene2 tables

../../doOmimGeneSymbols $db stdout | sort -u > omimGeneSymbol.tab

hgLoadSqlTab -verbose=0 -warn $db omimGeneSymbolNew ~/kent/src/hg/lib/omimGeneSymbol.sql omimGeneSymbol.tab  

./doOmimPhenotype.pl --gene-map-file=genemap.txt >omimPhenotype.tab

hgLoadSqlTab -verbose=0 -warn $db omimPhenotypeNew ~/kent/src/hg/lib/omimPhenotype.sql omimPhenotype.tab  

hgsql $db -e 'update omimPhenotypeNew set omimPhenoMapKey = -1 where omimPhenoMapKey=0'
hgsql $db -e 'update omimPhenotypeNew set phenotypeId = -1 where phenotypeId=0'

../../doOmimGene2 $db stdout | sort -u > omimGene2.tab

hgLoadBed -verbose=0 $db omimGene2New omimGene2.tab 

##############################################################
# build the omimAvSnp track

mkdir -p av
cd av

# If gene symbol (3rd field) is blank, replace it with "-".
# Reorganize fields - field 2 moved to the line end; fields 1 and 4 are each
# followed by fields with their contents split up. 
grep -v '^#' ../allelicVariants.txt | awk 'BEGIN {FS="\t"; OFS="\t"};
  $3 == "" {$3 = "-"};
  {$(NF+1)=$2;  $2=$1; sub(/\./, OFS, $2);
  repl=$4; if (sub(/,/, OFS, repl) == 0) repl=repl OFS repl; $4=$4 OFS repl;
  print $0}' > omimAv.tab

hgLoadSqlTab -verbose=0 -warn $db omimAvNew ~/kent/src/hg/lib/omimAv.sql omimAv.tab

# Remove whitespace; really this should probably be done with the initial processing,
# but this works.
hgsql $db -e 'update omimAvNew set repl2 = rtrim(ltrim(repl2))'

# doOmimAv needs work - it has several issues.
../../../doOmimAv $db omimAvRepl.tab j.err

hgLoadSqlTab -verbose=0 -warn $db omimAvReplNew ~/kent/src/hg/lib/omimAvRepl.sql omimAvRepl.tab 

if ($db == "hg18") then
  hgsql $db -N -e 'select chrom, chromStart, chromEnd, avId from omimAvReplNew r, snp130 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
else if ($db == "hg19") then
  hgsql $db -N -e 'select chrom, chromStart, chromEnd, avId from omimAvReplNew r, snp144 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
else if ($db == "hg38") then
  hgsql $db -N -e 'select chrom, chromStart, chromEnd, avId from omimAvReplNew r, snp144 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
else
  echo "Error in buildOmimTracks.csh: unable to construct omimAvSnp for $db.  Do not know which SNP table to use."
  exit 255;
endif

hgLoadBed -verbose=0 -allowStartEqualEnd  $db omimAvSnpNew omimAvSnp.tab 
cd ..

##############################################################
echo build omimLocation ...

mkdir -p location
cd location

# ../../../doOmimLocation $db omimLocation.bed j.err
../../../doOmimLocation $db omimLocation.bed 

hgLoadBed -verbose=0 $db omimLocationNew omimLocation.bed 

# Remove all gene entries in omimGene2 from omimLocation table

hgsql $db -N -e \
'delete from omimLocationNew where name  in (select name from omimGene2New) '

# Per OMIM request, delete all the gray entries in omimLocation table.

#mkdir -p cleanUpOmimLocation
#cd cleanUpOmimLocation

echo cleaning omimLocation ...

#  hgsql $db -N -e \
#  'select distinct name from omimLocationNew' |sort -u >j.all
#
#  hgsql $db -N -e \
#  'select distinct name from omimLocationNew, omimPhenotypeNew where name=omimId and omimPhenoMapKey=1' >j.1
#  hgsql $db -N -e \
#  'select distinct name from omimLocationNew, omimPhenotypeNew where name=omimId and omimPhenoMapKey=2' >j.2
#  hgsql $db -N -e \
#  'select distinct name from omimLocationNew, omimPhenotypeNew where name=omimId and omimPhenoMapKey=3' >j.3
#  hgsql $db -N -e \
#  'select distinct name from omimLocationNew, omimPhenotypeNew where name=omimId and omimPhenoMapKey=4' >j.4
#
#  cat j.1 j.2 j.3 j.4 |sort -u >j.1234
#
#  join -v 1 j.all j.1234 |sed -e "s/^/.\/do1  /" >doall

#cat doall

#cat << _EOF_ > do1
#hgsql $db -e 'delete from omimLocationNew where name="\$2"'
#_EOF_

hgsql $db -N -e 'delete from omimLocationNew using omimLocationNew left join omimPhenotypeNew on
    omimLocationNew.name = omimPhenotypeNew.omimId where omimPhenotypeNew.omimPhenoMapKey is null
    or omimPhenotypeNew.omimPhenoMapKey not in (1,2,3,4)'

#sleep 3
#echo after sleep

#chmod +x do1
#chmod +x doall
#./doall

##############################################################

# remember to check in new mim2gene.sql


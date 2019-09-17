#!/bin/sh -e
# BUILD $db OMIM RELATED TRACKS

set -eEu -o pipefail

export db=$1

############################################################
# Load genemap and genemap2 table

# Insert additional tabs to pad out fields that used to be split into multiple
# fields. (e.g., comments used to be comments1 and comments2)
grep -v '^#' genemap.txt | tawk \
   '{$8 = $8 OFS; $11 = $11 OFS; $12 = $12 OFS OFS; $13 = $13 OFS; print $0}' > genemap.tab

# quick hack - sometimes month or day are missing, and mysql complains
tawk '$2 == "" {$2 = 0}; $3 == "" {$3 = 0}; {print $0}' genemap.tab |
hgLoadSqlTab -verbose=0 -warn $db omimGeneMapNew ~/kent/src/hg/lib/omimGeneMap.sql stdin

# genemap2 contains chromosome coordinates but we map to refGene so forget about them
grep -v '^#' genemap2.txt | tawk \
    '
    {
    for (i=4; i <= NF; i++)
        {
        if (i != NF)
            {
            printf "%s\t", $i;
            }
        else
            {
            printf "%s\n", $i;
            }
        }
    }
    ' > genemap2.tab
hgLoadSqlTab -verbose=0 -warn $db omimGeneMap2New ~/kent/src/hg/lib/omimGeneMap2.sql genemap2.tab


############################################################
# Load mim2gene table and omimGene2
# note: doOmimGene2 depends on omim2gene

# split gene/phenotype entries, omit Ensembl gene IDs
grep -v '^#' mim2gene.txt | tawk '$2 ~ "gene/phenotype" \
  {$2 = "gene"; print $1,$2,$3,$4; $2 = "phenotype"}; {print $1,$2,$3,$4}' > mim2gene.updated.txt

# quick hack - frequently there's no gene ID and mysql complains
tawk '$3 == "" {$3 = 0}; {print $0}' mim2gene.updated.txt |
hgLoadSqlTab -verbose=0 -warn $db omim2geneNew ~/kent/src/hg/lib/omim2gene.sql stdin

# Not sure what this file is created for.  Can probably remove this?
tawk '{print $1, $3, $2}' mim2gene.updated.txt > mim2gene.tab

../../doOmimGene2 $db stdout | sort -u > omimGene2.tab

hgLoadBed -verbose=0 $db omimGene2New omimGene2.tab

############################################################
# build omimGeneSymbol and omimPhenotype tables
# note: doOmimGeneSymbol depends on omimGeneMap2 table

../../doOmimGeneSymbols $db stdout | sort -u > omimGeneSymbol.tab

hgLoadSqlTab -verbose=0 -warn $db omimGeneSymbolNew ~/kent/src/hg/lib/omimGeneSymbol.sql omimGeneSymbol.tab

# old version
#./doOmimPhenotype.pl -gene-map-file=genemap.txt >omimPhenotype.tab
./doOmimPhenotype.pl --gene-map-file=genemap2.txt > omimPhenotype.tab

# quick hack - sometimes phenotype ID is blank and mysql complains
tawk '$3 == "" {$3 = 0}; {print $0}' omimPhenotype.tab |
hgLoadSqlTab -verbose=0 -warn $db omimPhenotypeNew ~/kent/src/hg/lib/omimPhenotype.sql stdin

hgsql $db -e 'update omimPhenotypeNew set omimPhenoMapKey = -1 where omimPhenoMapKey=0'
hgsql $db -e 'update omimPhenotypeNew set phenotypeId = -1 where phenotypeId=0'


##############################################################
# build the omimAvSnp track

mkdir -p av
cd av

# If gene symbol (3rd field) is blank, replace it with "-".
# Reorganize fields - field 2 moved to the line end; fields 1 and 4 are each
# followed by fields with their contents split up.
grep -v '^#' ../allelicVariants.txt | tawk \
  '$3 == "" {$3 = "-"}; \
  {$(NF+1)=$2;  $2=$1; sub(/\./, OFS, $2); \
  repl=$4; if (sub(/,/, OFS, repl) == 0) repl=repl OFS repl; $4=$4 OFS repl; \
  print $0}' > omimAv.tab

hgLoadSqlTab -verbose=0 -warn $db omimAvNew ~/kent/src/hg/lib/omimAv.sql omimAv.tab

# Remove whitespace; really this should probably be done with the initial processing,
# but this works.
hgsql $db -e 'update omimAvNew set repl2 = rtrim(ltrim(repl2))'

if [ "$db" == "hg18" ]
then
#  hgsql $db -N -e 'select chrom, chromStart, chromEnd, avId from omimAvReplNew r, snp130 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
  snpTbl="snp130";
elif [ "$db" == "hg19" ]
then
#  hgsql $db -N -e 'select chrom, chromStart, chromEnd, avId from omimAvReplNew r, snp151 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
  snpTbl="snp151";
elif [ "$db" == "hg38" ]
then
#  hgsql $db -N -e 'select chrom, chromStart, chromEnd, avId from omimAvReplNew r, snp151 s where s.name = dbSnpId order by avId' |sort -u > omimAvSnp.tab
  snpTbl="snp151";
else
  echo "Error in buildOmimTracks.csh: unable to construct omimAvSnp for $db.  Do not know which SNP table to use."
  exit 255;
fi

tawk '{print $1, $8}' omimAv.tab | perl -ne 'chomp; m/^(\S+)/; $avId = $1; while (m/(rs\d+)(,|$)/g) {print "$avId\t$1\n"}' > omimAvLinkTmp.tab
hgsql $db -Ne 'drop table if exists omimAvLinkTmp'
hgsql $db -Ne 'create table omimAvLinkTmp (avId text not null, dbSnpId text not null, index(dbSnpId(10)))'
hgsql $db -Ne 'load data local infile "omimAvLinkTmp.tab" into table omimAvLinkTmp'
hgsql $db -N -e "select chrom, chromStart, chromEnd, avId from omimAvLinkTmp l, $snpTbl s where s.name = l.dbSnpId" | bedSort stdin omimAvSnp.tab
hgsql $db -Ne 'drop table omimAvLinkTmp'

hgLoadBed -verbose=0 -allowStartEqualEnd  $db omimAvSnpNew omimAvSnp.tab
cd ..

##############################################################
# note: doOmimLocation depends on omimGeneMap and omimGene2 and omimPhenotype
echo building omimLocation ...

mkdir -p location
cd location

# the new genemap2 file contains cytoband locations that don't exist in hg18,
# generally something like p32.33 in genemap2 where hg18 has p32.3, not too serious
# but keep the errors just in case
../../../doOmimLocation $db omimLocation.bed  2>omimLocation.errors

hgLoadBed -verbose=0 $db omimLocationNew omimLocation.bed

# Remove all gene entries in omimGene2 from omimLocation table
hgsql $db -N -e \
'delete from omimLocationNew where name  in (select name from omimGene2New) '

# Per OMIM request, delete all the gray entries in omimLocation table.
echo cleaning omimLocation ...

hgsql $db -N -e 'delete from omimLocationNew using omimLocationNew left join omimPhenotypeNew on
    omimLocationNew.name = omimPhenotypeNew.omimId where omimPhenotypeNew.omimPhenoMapKey is null
    or omimPhenotypeNew.omimPhenoMapKey not in (1,2,3,4)'


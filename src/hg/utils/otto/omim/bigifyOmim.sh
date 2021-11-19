#omimGenemap2ToInhTable 2020-05-18/genemap2.txt 2020-05-18/hg38/genemap2inheritance.tab
#doOmimPhenotypeInherit 2020-05-18/genemap2.txt 2020-05-18/hg38/omimPhenotype2.tab 
#hgLoadSqlTab -verbose=0 -warn hg38 omimPhenotypeNew ~/kent/src/hg/lib/omimPhenotype.sql 2020-05-18/hg38/omimPhenotype2.tab
#hgLoadSqlTab -verbose=0 -warn hg38 omimPhenotype ~/kent/src/hg/lib/omimPhenotype.sql 2020-05-18/hg38/omimPhenotype2.tab
#hgsql hg19 -e 'select chrom, chromStart, chromEnd, name, "0", ".", chromStart, chromEnd, "color", geneSymbol, disorders1, GROUP_CONCAT(omimPhenotype.description, "|", omimPhenoMapKey, "|", inhMode SEPARATOR "$") from omimGene2, omimGeneMap, omimPhen
#otype where omimGene2.name=omimGeneMap.omimId and omimGene2.name=omimPhenotype.omimId'
#python fixupBed.py  > omimGene2bb.bed
#bedToBigBed omimGene2bb.bed /hive/data/genomes/hg19/chrom.sizes omimGene2bb.bb -tab -as=omimGene2.as -type=bed9+
#bedToBigBed omimGene2bb.bed /hive/data/genomes/hg19/chrom.sizes omimGene2.bb -tab -as=omimGene2.as -type=bed9+
#bedSort omimGene2bb.bed omimGene2bb.bed
#bedToBigBed omimGene2bb.bed /hive/data/genomes/hg19/chrom.sizes omimGene2.bb -tab -as=omimGene2.as -type=bed9+

set -e
for db in hg19 hg38; do
hgsql $db -e 'select chrom, chromStart, chromEnd, name, "0", ".", chromStart, chromEnd, "color", geneSymbol, disorders1, GROUP_CONCAT(omimPhenotype.description, "|", omimPhenoMapKey, "|", inhMode SEPARATOR "$"), omimPhenoMapKey from omimGene2, omimGeneMap, omimPhenotype where omimGene2.name=omimGeneMap.omimId and omimGene2.name=omimPhenotype.omimId GROUP BY chrom, chromStart, chromEnd, omimGeneMap.omimId ' -NB  > omimGene2.tmp
hgsql $db -NB -e 'select chrom, chromStart, chromEnd, name, "0", ".", chromStart, chromEnd, "color", geneSymbol, "", "", "0" from omimGene2 JOIN omimGeneMap ON omimGene2.name=omimGeneMap.omimId LEFT OUTER JOIN omimPhenotype on omimGene2.name=omimPhenotype.omimId where phenotypeId is NULL' >> omimGene2.tmp
bedSort omimGene2.tmp omimGene2.tmp
python omimGene2ToBigBed.py omimGene2.tmp /hive/data/genomes/$db/chrom.sizes 2020-10-22/omimGene2.$db.bb 
#rm omimGene2.tmp 
rm -f /gbdb/$db/bbi/omimGene2.bb
ln -s `pwd`/2020-10-22/omimGene2.$db.bb /gbdb/$db/bbi/omimGene2.bb 
done

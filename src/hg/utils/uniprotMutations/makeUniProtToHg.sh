#!/bin/sh
# create mapping from uniProt protein positions to genome positions

# *ALL* was stolen from Markd's script LS-SNP pipeline
# file: snpProtein/build/Makefile

hgDb=19

# if you change BLASTDIR, also must change the cluster script mapUniprot_doBlast
BLASTDIR=/cluster/bin/blast/x86_64/blast-2.2.16/bin

# this was created by the uniprot parser as part of the publications source code
UNIPROTFAGZ=/hive/data/inside/pubs/parsedDbs/uniprot.9606.fa.gz

# stop on errors
set -e
# show commands
set -x 

rm -rf upMap/work/*
mkdir -p upMap/work

# get data
zcat ${UNIPROTFAGZ} > upMap/work/uniProt.fa
hgsql -Ne 'select name, chrom, strand, txStart, txEnd, cdsStart, cdsEnd, exonCount, exonStarts, exonEnds from knownGene where cdsStart < cdsEnd' hg$hgDb  > upMap/work/ucscGene.gp
genePredToFakePsl hg$hgDb upMap/work/ucscGene.gp upMap/work/ucscMRna.psl upMap/work/ucscMRna.cds
hgsql -Ne 'select spID,kgID from kgXref where spID!=""' hg$hgDb > upMap/work/ucscUniProt.pairs
getRnaPred hg$hgDb upMap/work/ucscGene.gp all upMap/work/ucscMRna.fa

# setup blast uniprot -> ucsc genes
mkdir upMap/work/queries 
mkdir upMap/work/aligns 
faSplit about upMap/work/uniProt.fa 2500 upMap/work/queries/
${BLASTDIR}/formatdb -i upMap/work/ucscMRna.fa -p F

# create joblist and run
set +x # quiet for now
>upMap/work/jobList
for i in upMap/work/queries/*.fa; do
        echo "../../mapUniprot_doBlast ucscMRna.fa queries/`basename $i` {check out exists aligns/`basename $i .fa`.psl}" >> upMap/work/jobList
done; 
set -x
ssh swarm "cd `pwd`/upMap/work && para make jobList"

# sort, pick the best alignments for each protein and then pslMap through them
find upMap/work/aligns -name '*.psl' | xargs cat | pslSelect -qtPairs=upMap/work/ucscUniProt.pairs stdin stdout | sort -k 14,14 -k 16,16n -k 17,17n > upMap/work/uniProtVsUcscMRna.psl
pslMap upMap/work/uniProtVsUcscMRna.psl upMap/work/ucscMRna.psl upMap/work/uniProtVsGenome.psl
sort -k10,10 upMap/work/uniProtVsGenome.psl | pslCDnaFilter stdin -globalNearBest=0  -bestOverlap -filterWeirdOverlapped stdout | sort | uniq > ./uniProtToHg$hgDb.psl

#!/bin/bash
set -beEu -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateIdMapping.sh

usage() {
    echo "usage: $0 metadata_date.tsv.gz sequences_date.fasta.gz"
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

nextmeta=$1
nextfasta=$2

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)
ottoDir=/hive/data/outside/otto/sarscov2phylo
mapScriptDir=~angie/chris_ncov
# Should use a better location than this...
installDir=/hive/users/angie/gisaid

ncbiDir=$ottoDir/ncbi.$today
cogUkDir=$ottoDir/cogUk.$today
cncbDir=$ottoDir/cncb.latest

# Set up input files for Chris's scripts to map GISAID <--> public sequences
cd $mapScriptDir
rm -rf input/$today
mkdir input/$today
cd input/$today
ln -sf $cncbDir/cncb.nonGenBank.fasta.xz .
ln -sf $ncbiDir/genbank.fa.xz .
ln -sf $cogUkDir/cog_all.fasta.xz .
ln -sf $nextfasta .
xcat $nextmeta | tail -n+2 | cut -f1,3 | uniq > seqToEpi

cd $mapScriptDir
time ./build.sh -t $today

cd $installDir

gbToDate=$ncbiDir/gbToDate
cogUkToDate=$cogUkDir/cogUkToDate
cncbToDate=$cncbDir/cncbToDate

join -t$'\t' -a 1 -1 2 -o 1.1,1.2,1.3,2.2 \
    <(sort -k2,2 $mapScriptDir/epiToPublicIdName.$today.txt) \
    <(sort $gbToDate $cncbToDate $cogUkToDate) \
| sort -u adders - \
    > epiToPublicAndDate.$today

# Look for duplicate ID swizzles relative to existing ncov-ingest mappings
cut -f 1,2 epiToPublicAndDate.$today \
| egrep $'\t''[A-Z][A-Z][0-9]+\.[0-9]+' \
| sort > latestEpiToGb
zcat ~angie/github/ncov-ingest/source-data/accessions.tsv.gz \
| tail -n+2 \
| tawk '{print $2, $1;}' \
| sort > ncovEpiToGb
wc -l latestEpiToGb ncovEpiToGb
# But allow version updates (e.g. .1 --> .2)
join -t$'\t' ncovEpiToGb latestEpiToGb \
| tawk '{ ncovNoDot = substr($2, 0, index($2, ".")-1);
          latestNoDot = substr($3, 0, index($3, ".")-1);
          if (ncovNoDot != latestNoDot) {print $3, $2;} }' \
    > latestToNcov.sub
# But watch out for name mismatches which indicate bad name matches; get rid of those.
tawk '{print $1, $1, $2, $2;}' latestToNcov.sub \
| subColumn -skipMiss 2 stdin <(cut -f 1,6 $ncbiDir/ncbi_dataset.plusBioSample.tsv) stdout \
| subColumn -skipMiss 4 stdin <(cut -f 1,6 $ncbiDir/ncbi_dataset.plusBioSample.tsv) stdout \
| tawk '$2 != $4' > latestToNcov.nameMismatch
grep -vFwf <(cut -f 1 latestToNcov.nameMismatch) latestToNcov.sub > tmp
mv tmp latestToNcov.sub

subColumn -miss=/dev/null 2 epiToPublicAndDate.$today latestToNcov.sub tmp
mv tmp epiToPublicAndDate.$today

ln -sf epiToPublicAndDate.$today epiToPublicAndDate.latest

# Now that mapping is updated, add GenBank accessions to 4th column of metadata
zcat $nextmeta | cut -f 4 | grep ... | wc -l
set +o pipefail
zcat $nextmeta | head -1 > tmp
set -o pipefail
zcat $nextmeta \
| tail -n+2 \
| sort -t $'\t' -k3,3 \
| join -t$'\t' -a 1 -1 3 - <(tawk '$2 ~ /^[A-Z][A-Z][0-9]+/' epiToPublicAndDate.latest) \
    -o 1.1,1.2,1.3,2.2,1.5,1.6,1.7,1.8,1.9,1.10,1.11,1.12,1.13,1.14,1.15,1.16,1.17,1.18,1.19,1.20,1.21,1.22,1.23,1.24,1.25,1.26,1.27 \
| sort \
    >> tmp
wc -l tmp
pigz -p 8 tmp
mv tmp.gz $nextmeta
zcat $nextmeta | cut -f 4 | grep ... | wc -l

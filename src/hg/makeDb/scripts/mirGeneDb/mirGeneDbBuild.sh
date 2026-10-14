#!/bin/bash
# Build the mirGeneDb bigBed and its arm decorator bigBed for hg38.
# Run from /hive/data/genomes/hg38/bed/mirGeneDb after mirGeneDbDownload.sh
# has filled the download/ subdirectory.
# The pri-miRNA sequences are taken from hg38 at the MirGeneDB coordinates and
# folded with RNAfold -d2 (ViennaRNA).
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
dl=download
twoBit=/hive/data/genomes/hg38/hg38.2bit
sizes=/hive/data/genomes/hg38/chrom.sizes

python3 $scriptDir/mirGeneDbToBed.py priFa $dl/hsa-all.bed $twoBit pri.fa
RNAfold --version > rnafold.version
RNAfold -d2 --noPS < pri.fa > pri.fold
python3 $scriptDir/mirGeneDbToBed.py bed $dl/hsa-all.bed $dl/hsa.gff $dl/hsa.browse.html \
    pri.fold $twoBit mirGeneDb.unsorted.bed mirGeneDbArms.unsorted.bed
sort -k1,1 -k2,2n mirGeneDb.unsorted.bed > mirGeneDb.bed
sort -k1,1 -k2,2n mirGeneDbArms.unsorted.bed > mirGeneDbArms.bed
rm mirGeneDb.unsorted.bed mirGeneDbArms.unsorted.bed

bedToBigBed -tab -type=bed6+12 -as=$scriptDir/mirGeneDb.as -extraIndex=name \
    mirGeneDb.bed $sizes mirGeneDb.bb
bedToBigBed -tab -type=bed12+4 -as=$scriptDir/../../../lib/decoration.as \
    mirGeneDbArms.bed $sizes mirGeneDbArms.bb

# search index, case-insensitive: MirGeneDB ID, the gene name without the variant
# (-vN), miRBase ID, family, MI and MIMAT accessions
awk -F'\t' '{g=$4; sub(/-v[0-9]+$/, "", g); print $4, g, $7, $8, $10, $11, $12}' mirGeneDb.bed \
    > mirGeneDb.ix.txt
ixIxx mirGeneDb.ix.txt mirGeneDb.ix mirGeneDb.ixx

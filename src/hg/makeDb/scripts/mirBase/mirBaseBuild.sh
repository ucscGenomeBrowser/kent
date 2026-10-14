#!/bin/bash
# Build the mirBase bigBed and its mature miRNA decorator bigBed for hg38.
# Run from /hive/data/genomes/hg38/bed/mirBase after mirBaseDownload.sh
# has put hsa.gff3 into the download/ subdirectory.
# The precursor sequences are taken from hg38 at the miRBase coordinates and
# folded with RNAfold -d2 (ViennaRNA).
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
gff=download/hsa.gff3
twoBit=/hive/data/genomes/hg38/hg38.2bit
sizes=/hive/data/genomes/hg38/chrom.sizes

python3 $scriptDir/mirBaseToBed.py preFa $gff $twoBit pre.fa
RNAfold --version > rnafold.version
RNAfold -d2 --noPS < pre.fa > pre.fold
python3 $scriptDir/mirBaseToBed.py bed $gff pre.fold \
    mirBase.unsorted.bed mirBaseArms.unsorted.bed
sort -k1,1 -k2,2n mirBase.unsorted.bed > mirBase.bed
sort -k1,1 -k2,2n mirBaseArms.unsorted.bed > mirBaseArms.bed
rm mirBase.unsorted.bed mirBaseArms.unsorted.bed

bedToBigBed -tab -type=bed6+10 -as=$scriptDir/mirBase.as -extraIndex=name \
    mirBase.bed $sizes mirBase.bb
bedToBigBed -tab -type=bed12+4 -as=$scriptDir/../../../lib/decoration.as \
    mirBaseArms.bed $sizes mirBaseArms.bb

# search index, case-insensitive: precursor name, MI accession, mature names and
# MIMAT accessions
awk -F'\t' '{print $4, $7, $8, $9, $10, $11}' mirBase.bed | tr ',' ' ' > mirBase.ix.txt
ixIxx mirBase.ix.txt mirBase.ix mirBase.ixx

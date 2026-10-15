#!/bin/bash
# Build the mirGeneDb bigBed and its arm decorator bigBed for one assembly.
# usage: mirGeneDbBuild.sh [db [species]]     default: hg38 hsa
# Run from /hive/data/genomes/<db>/bed/mirGeneDb after mirGeneDbDownload.sh
# has filled the download/ subdirectory (for a lifted assembly, with the lifted
# <species>-all.bed, see mirGeneDbLift.py).
# The pri-miRNA sequences are taken from the assembly at the MirGeneDB coordinates and
# folded with RNAfold -d2 (ViennaRNA); pri-miRNAs whose precursor is not one hairpin are
# refolded with the flanks unpaired in mirGeneDbToBed.py (RNAfold, RNAsubopt).
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
db=${1:-hg38}
sp=${2:-hsa}
dl=download
twoBit=/hive/data/genomes/$db/$db.2bit
sizes=/hive/data/genomes/$db/chrom.sizes

python3 $scriptDir/mirGeneDbToBed.py priFa $dl/$sp-all.bed $twoBit pri.fa
RNAfold --version > rnafold.version
RNAfold -d2 --noPS < pri.fa > pri.fold
python3 $scriptDir/mirGeneDbToBed.py bed $dl/$sp-all.bed $dl/$sp.gff $dl/$sp.browse.html \
    pri.fold $twoBit mirGeneDb.unsorted.bed mirGeneDbArms.unsorted.bed
sort -k1,1 -k2,2n mirGeneDb.unsorted.bed > mirGeneDb.bed
sort -k1,1 -k2,2n mirGeneDbArms.unsorted.bed > mirGeneDbArms.bed
rm mirGeneDb.unsorted.bed mirGeneDbArms.unsorted.bed

bedToBigBed -tab -type=bed6+13 -as=$scriptDir/mirGeneDb.as -extraIndex=name \
    mirGeneDb.bed $sizes mirGeneDb.bb
bedToBigBed -tab -type=bed12+4 -as=$scriptDir/../../../lib/decoration.as \
    mirGeneDbArms.bed $sizes mirGeneDbArms.bb

# search index, case-insensitive: MirGeneDB ID, the gene name without the variant
# (-vN), miRBase ID, family, MI and MIMAT accessions
awk -F'\t' '{g=$4; sub(/-v[0-9]+$/, "", g); print $4, g, $7, $8, $10, $11, $12}' mirGeneDb.bed \
    > mirGeneDb.ix.txt
ixIxx mirGeneDb.ix.txt mirGeneDb.ix mirGeneDb.ixx

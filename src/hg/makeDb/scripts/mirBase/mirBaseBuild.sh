#!/bin/bash
# Build the mirBase bigBed and its mature miRNA decorator bigBed for one assembly.
# usage: mirBaseBuild.sh [db [species [mirGeneDb.bed]]]     default: hg38 hsa
#   mirGeneDb.bed: the MirGeneDB track of the assembly, for the MirGeneDB IDs that
#   MirGeneDB gives each miRBase accession (default: the mirGeneDbV3_0 build directory)
# Run from /hive/data/genomes/<db>/bed/mirBase after mirBaseDownload.sh
# has put <species>.gff3 into the download/ subdirectory (for a lifted assembly, the
# lifted GFF, see mirBaseLift.py).
# The precursor sequences are taken from the assembly at the miRBase coordinates and
# folded with RNAfold -d2 (ViennaRNA); precursors that are not one hairpin get the best
# single-hairpin suboptimal structure in mirBaseToBed.py (RNAsubopt).
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
db=${1:-hg38}
sp=${2:-hsa}
mirGeneDbBed=${3:-/hive/data/genomes/$db/bed/mirGeneDbV3_0/mirGeneDb.bed}
gff=download/$sp.gff3
twoBit=/hive/data/genomes/$db/$db.2bit
sizes=/hive/data/genomes/$db/chrom.sizes

python3 $scriptDir/mirBaseToBed.py preFa $gff $twoBit pre.fa
RNAfold --version > rnafold.version
RNAfold -d2 --noPS < pre.fa > pre.fold
python3 $scriptDir/mirBaseToBed.py bed $gff pre.fold $mirGeneDbBed \
    mirBase.unsorted.bed mirBaseArms.unsorted.bed
sort -k1,1 -k2,2n mirBase.unsorted.bed > mirBase.bed
sort -k1,1 -k2,2n mirBaseArms.unsorted.bed > mirBaseArms.bed
rm mirBase.unsorted.bed mirBaseArms.unsorted.bed

bedToBigBed -tab -type=bed6+16 -as=$scriptDir/mirBase.as -extraIndex=name \
    mirBase.bed $sizes mirBase.bb
bedToBigBed -tab -type=bed12+4 -as=$scriptDir/../../../lib/decoration.as \
    mirBaseArms.bed $sizes mirBaseArms.bb

# search index, case-insensitive: precursor name, MI accession, mature names and
# MIMAT accessions
awk -F'\t' '{print $4, $9, $10, $13, $14, $17}' mirBase.bed | tr ',' ' ' \
    | awk '{s = $1; for (i = 2; i <= NF; i++) if ($i != "-" && $i != "|-") s = s " " $i; print s}' \
    > mirBase.ix.txt
ixIxx mirBase.ix.txt mirBase.ix mirBase.ixx

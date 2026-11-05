#!/bin/bash
# Write a parasol jobList for the SPARK WGS 2026_08 pVCFs: every 2.5 Mb chunk
# is split into pieces of <pieceSize> bp, and each piece is one job that runs
# sparkWgs45kPvcfToSites.sh on it.
# Usage: sparkWgs45kPvcfJobs.sh <pvcfDir> <groups.txt> <outDir> [pieceSize] > jobList
#   pvcfDir: holds <chrom>/SPARK.WGS.2026_08.gatk.<chrom>_<start>_<end>.vcf.gz
#   outDir:  gets <chrom>/<chrom>_<pieceStart>.bcf (+ .csi, .norm.log)
set -euo pipefail

pvcfDir=$(readlink -f "$1")
groups=$(readlink -f "$2")
outDir=$(readlink -f "$3")
pieceSize=${4:-500000}
scriptDir=$(dirname "$(readlink -f "$0")")

for f in "$pvcfDir"/chr*/*.vcf.gz; do
    base=$(basename "$f" .vcf.gz)
    region=${base##*.}                       # e.g. chr21_40000001_42500000
    chrom=${region%%_*}
    rest=${region#*_}
    start=${rest%_*}
    end=${rest#*_}
    mkdir -p "$outDir/$chrom"
    for ((s = start; s <= end; s += pieceSize)); do
        e=$(( s + pieceSize < end + 1 ? s + pieceSize : end + 1 ))
        out=$outDir/$chrom/${chrom}_$(printf '%09d' $s).bcf
        echo "$scriptDir/sparkWgs45kPvcfToSites.sh $f $groups $out $s $e {check out exists $out}"
    done
done

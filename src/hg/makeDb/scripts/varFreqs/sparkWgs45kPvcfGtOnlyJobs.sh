#!/bin/bash
# Write a parasol jobList that reduces every 500 kb piece of the SPARK WGS
# 2026_08 pVCF chunks to genotype-only VCF with sparkWgs45kPvcfGtOnly.sh.
# Usage: sparkWgs45kPvcfGtOnlyJobs.sh <pvcfDir> <outDir> [pieceSize] > jobList
#   outDir gets <chrom>/<chrom>_<pieceStart>.vcf.gz
set -euo pipefail

pvcfDir=$(readlink -f "$1")
outDir=$(readlink -f "$2")
pieceSize=${3:-500000}
scriptDir=$(dirname "$(readlink -f "$0")")

for f in "$pvcfDir"/chr*/*.vcf.gz; do
    region=$(basename "$f" .vcf.gz)
    region=${region##*.}                     # e.g. chr21_40000001_42500000
    chrom=${region%%_*}
    rest=${region#*_}
    start=${rest%_*}
    end=${rest#*_}
    mkdir -p "$outDir/$chrom"
    for ((s = start; s <= end; s += pieceSize)); do
        e=$(( s + pieceSize < end + 1 ? s + pieceSize : end + 1 ))
        out=$outDir/$chrom/${chrom}_$(printf '%09d' $s).vcf.gz
        echo "$scriptDir/sparkWgs45kPvcfGtOnly.sh $f $out $s $e {check out exists $out}"
    done
done

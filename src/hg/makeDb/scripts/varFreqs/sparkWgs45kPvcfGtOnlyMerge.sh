#!/bin/bash
# Join the genotype-only pieces made by sparkWgs45kPvcfGtOnly.sh into one
# bgzipped VCF and tabix-index it. The pieces have identical headers and do not
# overlap (records are not left-aligned), so bcftools concat --naive can copy
# the compressed blocks without decompressing them.
# Usage: sparkWgs45kPvcfGtOnlyMerge.sh <piecesDir> <out.vcf.gz>
set -euo pipefail

piecesDir=$1
outVcf=$2
B=/cluster/software/src/bcftools-1.22/bcftools

for c in $(ls "$piecesDir" | sort -V); do
    ls "$piecesDir/$c"/*.vcf.gz
done > "$outVcf.files.txt"
$B concat --naive -f "$outVcf.files.txt" -o "$outVcf.tmp.vcf.gz"
mv -f "$outVcf.tmp.vcf.gz" "$outVcf"
tabix -p vcf "$outVcf"
rm -f "$outVcf.files.txt"

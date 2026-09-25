#!/bin/bash
# Merge per-piece sites BCFs made by sparkWgs45kPvcfToSites.sh into one sorted,
# bgzipped, tabix-indexed VCF.
# Usage: sparkWgs45kPvcfMerge.sh <out.vcf.gz> <piece1.bcf> [piece2.bcf ...]
# The pieces must be given in position order. Left-alignment can move a record
# a few bases before its piece start, so the concatenation is sorted again.
# Protein consequences (BCSQ) are not added here, only later when the cohorts
# are merged by mergeAndAnnotate.sh.
set -euo pipefail

outVcf=$1
shift
# not the conda bcftools in ~max/software: it links libopenblas, which crashes
# under the parasol -ram address-space limit
BCFTOOLS=/cluster/software/src/bcftools-1.22/bcftools
tmpDir=$(mktemp -d "$outVcf.tmpXXXX")

printf '%s\n' "$@" > "$tmpDir/files.txt"
$BCFTOOLS concat -f "$tmpDir/files.txt" -Ou \
  | $BCFTOOLS sort -T "$tmpDir/sort" -m 4G -Oz -o "$tmpDir/out.vcf.gz"
tabix -p vcf "$tmpDir/out.vcf.gz"
mv -f "$tmpDir/out.vcf.gz" "$outVcf"
mv -f "$tmpDir/out.vcf.gz.tbi" "$outVcf.tbi"
rm -rf "$tmpDir"

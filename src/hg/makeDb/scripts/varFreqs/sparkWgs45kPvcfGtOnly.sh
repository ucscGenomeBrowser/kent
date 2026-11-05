#!/bin/bash
# Reduce a position range of one SPARK WGS 2026_08 GLnexus pVCF chunk to a
# genotype-only VCF: CHROM POS REF ALT QUAL FILTER, INFO/AQ, and FORMAT/GT for
# all samples. ID (= CHROM_POS_REF_ALT) and INFO/AF (recomputable) are dropped,
# as are FORMAT DP AD SB GQ PL RNC. Records are not split or normalized.
# Usage: sparkWgs45kPvcfGtOnly.sh <in.vcf.gz> <out.vcf.gz> <start> <end>
#   start/end: 1-based, [start, end)
set -euo pipefail

inVcf=$1
outVcf=$2
start=$3
end=$4
scriptDir=$(dirname "$(readlink -f "$0")")
# not the conda bcftools: it links libopenblas, which crashes under parasol -ram
B=/cluster/software/src/bcftools-1.22/bcftools
tmp=$outVcf.tmp.vcf.gz

python3 "$scriptDir/sparkWgs45kPvcfSlice.py" "$inVcf" "$start" "$end" \
  | perl -pe 's/:[^\t\n]*//g unless /^#/' \
  | $B annotate -x ID,INFO/AF,INFO/AC,INFO/AN,FORMAT/DP,FORMAT/AD,FORMAT/SB,FORMAT/GQ,FORMAT/PL,FORMAT/RNC \
        -Oz -o "$tmp"
mv -f "$tmp" "$outVcf"

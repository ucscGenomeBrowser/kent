#!/usr/bin/env bash
# Build the abelSv bigBed for hg38 from the public Abel et al. 2020 CCDG
# structural-variant VCFs (B38 native + B37 lifted to hg38).
#
# Expects to be run from /hive/data/genomes/hg38/bed/abelSv/ (cwd) with
# Build38.public.v2.vcf.gz and Build37.public.v2.vcf.gz already downloaded
# (see makeDoc for download URLs).

set -euo pipefail

SCRIPTS=/cluster/home/max/kent/src/hg/makeDb/scripts/abelSv
CHAIN=/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz
CHROMSIZES=/hive/data/genomes/hg38/chrom.sizes
NTHREADS=8

# --- B38: native GRCh38 ---
echo "[$(date +%T)] processing B38 VCF..."
zcat Build38.public.v2.vcf.gz \
    | "$SCRIPTS/vcfToBed.py" B38 \
    > B38.bed

# --- B37: lift to hg38 ---
echo "[$(date +%T)] processing B37 VCF..."
zcat Build37.public.v2.vcf.gz \
    | "$SCRIPTS/vcfToBed.py" B37 \
    > B37.prelift.bed

echo "[$(date +%T)] lifting B37 to hg38..."
# bed has 35 columns; we use -bedPlus=9 -tab so liftOver passes extra
# columns through unchanged.
liftOver -tab -bedPlus=9 B37.prelift.bed "$CHAIN" \
    B37lift.bed B37.unmapped.bed

echo "  B37 mapped:    $(wc -l < B37lift.bed)"
echo "  B37 unmapped:  $(grep -c -v '^#' B37.unmapped.bed || true)"

# --- merge + sort ---
echo "[$(date +%T)] merging and sorting..."
cat B38.bed B37lift.bed \
    | sort -k1,1 -k2,2n --parallel="$NTHREADS" \
    > abelSv.bed

echo "  total variants: $(wc -l < abelSv.bed)"

# --- bigBed ---
echo "[$(date +%T)] building bigBed..."
bedToBigBed -type=bed9+26 -tab -as="$SCRIPTS/abelSv.as" \
    abelSv.bed "$CHROMSIZES" abelSv.bb

ls -lh abelSv.bb
echo "[$(date +%T)] done."

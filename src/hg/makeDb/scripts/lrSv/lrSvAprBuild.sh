#!/usr/bin/env bash
# Build apr lrSv subtrack for hs1 (native) and hg38 (lifted).
# Run from /hive/data/genomes/hg38/bed/lrSv/apr/

set -euo pipefail

SCRIPTS=/cluster/home/max/kent/src/hg/makeDb/scripts/lrSv
AS="$SCRIPTS/lrSvApr.as"
VCF=apr_review_v1_2902_chm13.vcf.gz
HS1_SIZES=/hive/data/genomes/hs1/chrom.sizes
HG38_SIZES=/hive/data/genomes/hg38/chrom.sizes
CHAIN=/gbdb/hs1/liftOver/hs1ToHg38.over.chain.gz

echo "[$(date +%T)] converting VCF to hs1 bed..."
zcat "$VCF" | python3 "$SCRIPTS/lrSvAprVcfToBed.py" /dev/stdin apr.hs1.bed "$HS1_SIZES"

echo "[$(date +%T)] sorting hs1 bed..."
bedSort apr.hs1.bed apr.hs1.sorted.bed

echo "[$(date +%T)] building hs1 bigBed..."
bedToBigBed -type=bed9+ -tab -as="$AS" \
    apr.hs1.sorted.bed "$HS1_SIZES" apr.hs1.bb

echo "[$(date +%T)] lifting to hg38..."
liftOver -tab -bedPlus=9 apr.hs1.bed "$CHAIN" \
    apr.hg38.bed apr.hs1.unmapped.bed
echo "  hg38 mapped:   $(wc -l < apr.hg38.bed)"
echo "  hg38 unmapped: $(grep -cv '^#' apr.hs1.unmapped.bed || true)"

echo "[$(date +%T)] sorting hg38 bed..."
bedSort apr.hg38.bed apr.hg38.sorted.bed

echo "[$(date +%T)] building hg38 bigBed..."
bedToBigBed -type=bed9+ -tab -as="$AS" \
    apr.hg38.sorted.bed "$HG38_SIZES" apr.hg38.bb

ls -lh apr.hs1.bb apr.hg38.bb
echo "[$(date +%T)] done."

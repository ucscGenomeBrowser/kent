#!/usr/bin/env bash
# Build cpc1 lrSv subtrack for hs1 (native) and hg38 (lifted).
# Run from /hive/data/genomes/hg38/bed/lrSv/cpc1/

set -euo pipefail

SCRIPTS=/cluster/home/max/kent/src/hg/makeDb/scripts/lrSv
AS="$SCRIPTS/lrSvCpc1.as"
VCF=CPC.HPRC.Phase1.processed.SVs.normed.vcf.gz
HS1_SIZES=/hive/data/genomes/hs1/chrom.sizes
HG38_SIZES=/hive/data/genomes/hg38/chrom.sizes
CHAIN=/gbdb/hs1/liftOver/hs1ToHg38.over.chain.gz

# --- Build hs1 native bed ---
echo "[$(date +%T)] converting VCF to hs1 bed..."
zcat "$VCF" | python3 "$SCRIPTS/lrSvCpc1VcfToBed.py" /dev/stdin cpc1.hs1.bed "$HS1_SIZES"

echo "[$(date +%T)] sorting hs1 bed..."
bedSort cpc1.hs1.bed cpc1.hs1.sorted.bed

echo "[$(date +%T)] building hs1 bigBed..."
bedToBigBed -type=bed9+ -tab -as="$AS" \
    cpc1.hs1.sorted.bed "$HS1_SIZES" cpc1.hs1.bb

# --- liftOver to hg38 ---
echo "[$(date +%T)] lifting to hg38..."
liftOver -tab -bedPlus=9 cpc1.hs1.bed "$CHAIN" \
    cpc1.hg38.bed cpc1.hs1.unmapped.bed

echo "  hg38 mapped:   $(wc -l < cpc1.hg38.bed)"
echo "  hg38 unmapped: $(grep -cv '^#' cpc1.hs1.unmapped.bed || true)"

echo "[$(date +%T)] sorting hg38 bed..."
bedSort cpc1.hg38.bed cpc1.hg38.sorted.bed

echo "[$(date +%T)] building hg38 bigBed..."
bedToBigBed -type=bed9+ -tab -as="$AS" \
    cpc1.hg38.sorted.bed "$HG38_SIZES" cpc1.hg38.bb

ls -lh cpc1.hs1.bb cpc1.hg38.bb
echo "[$(date +%T)] done."

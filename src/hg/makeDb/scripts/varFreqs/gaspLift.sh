#!/bin/bash
# gaspLift.sh - lift the GenomeAsia Pilot 100k VCFs from GRCh37 to hg38
# usage: gaspLift.sh <inVcf.gz> <outBase>
# outputs: <outBase>.vcf.gz (sorted, bgzipped) and .tbi
#
# Recipe (same as tishkoff180):
#   bare-name -> chr-prefix rename -> CrossMap.py vcf with hg19ToHg38
#   -> drop alt/random/Un/fix landings -> fix contig= header lines
#   -> bcftools sort -> bgzip + tabix.
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <inVcf.gz> <outBase>" >&2
    exit 1
fi

IN_VCF="$1"
OUT_BASE="$2"
WORK_DIR="$(dirname "$OUT_BASE")"
TMP_BASE="$WORK_DIR/$(basename "$OUT_BASE").tmp"

CHAIN=/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz
HG38_FA=/hive/data/genomes/hg38/bed/varFreqs/all/hg38.fa
RENAME=/hive/data/genomes/hg38/bed/varFreqs/ga100k/lift/chrRename.txt

mkdir -p "$WORK_DIR"

echo "[$(date '+%H:%M:%S')] Step 1: rename bare chromosomes -> chr-prefixed"
# The indel VCF has a malformed #CHROM line: it declares FORMAT but has no
# sample columns and no FORMAT field on data lines. Strip the trailing
# FORMAT before bcftools sees it. NF=8 is a no-op for clean sites-only files.
zcat "$IN_VCF" \
  | awk 'BEGIN{OFS="\t"} /^#CHROM/{NF=8} {print}' \
  | bcftools annotate --rename-chrs "$RENAME" --threads 4 -Oz \
    -o "$TMP_BASE.chr.vcf.gz" -

echo "[$(date '+%H:%M:%S')] Step 2: CrossMap.py vcf hg19 -> hg38"
CrossMap.py vcf "$CHAIN" \
    "$TMP_BASE.chr.vcf.gz" \
    "$HG38_FA" \
    "$TMP_BASE.hg38.vcf" \
    2> "$TMP_BASE.crossmap.log"
tail -20 "$TMP_BASE.crossmap.log"

echo "[$(date '+%H:%M:%S')] Step 3: filter to canonical chr1-22, X, Y, M and fix contig= header"
awk 'BEGIN{OFS="\t"}
     /^##contig=/ {sub(/<ID=/,"<ID=chr"); print; next}
     /^#/ {print; next}
     $1 ~ /^chr([1-9]|1[0-9]|2[0-2]|X|Y|M)$/ {print}' \
    "$TMP_BASE.hg38.vcf" > "$TMP_BASE.canon.vcf"

echo "[$(date '+%H:%M:%S')] Step 4: bcftools sort"
bcftools sort "$TMP_BASE.canon.vcf" -Oz -m 8G -T /data/tmp/ \
    -o "$OUT_BASE.vcf.gz"

echo "[$(date '+%H:%M:%S')] Step 5: tabix"
tabix -p vcf "$OUT_BASE.vcf.gz"

echo "[$(date '+%H:%M:%S')] Counts:"
INPUT_N=$(zcat "$IN_VCF" | grep -cv '^#' || true)
LIFTED_N=$(grep -cv '^#' "$TMP_BASE.hg38.vcf" || true)
UNMAP_N=$([ -f "$TMP_BASE.hg38.vcf.unmap" ] && grep -cv '^#' "$TMP_BASE.hg38.vcf.unmap" || echo 0)
FINAL_N=$(zcat "$OUT_BASE.vcf.gz" | grep -cv '^#' || true)
DROPPED_NONCANON=$((LIFTED_N - FINAL_N))
echo "  input         : $INPUT_N"
echo "  lifted        : $LIFTED_N"
echo "  unmapped      : $UNMAP_N"
echo "  drop non-canon: $DROPPED_NONCANON"
echo "  final         : $FINAL_N"

# clean up temp files (keep .crossmap.log and .unmap for QA)
rm -f "$TMP_BASE.chr.vcf.gz" "$TMP_BASE.hg38.vcf" "$TMP_BASE.canon.vcf"

echo "[$(date '+%H:%M:%S')] Done. Output: $OUT_BASE.vcf.gz"

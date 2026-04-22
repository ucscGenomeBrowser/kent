#!/usr/bin/env bash
# Build colorsDbSv bigBeds for hg38 and hs1 from the pbsv+Jasmine source
# VCFs distributed by the CoLoRSdb project.
#
# Run from /hive/data/genomes/hg38/bed/lrSv/colorsDb/.

set -euo pipefail

SCRIPTS=/cluster/home/max/kent/src/hg/makeDb/scripts/lrSv
AS="$SCRIPTS/lrSvColorsDbSv.as"
CONV="$SCRIPTS/lrSvColorsDbSvVcfToBed.py"
HG38_VCF=CoLoRSdb.GRCh38.v1.2.0.pbsv.jasmine.vcf.gz
HS1_VCF=CoLoRSdb.CHM13.v1.2.0.pbsv.jasmine.vcf.gz
HG38_SIZES=/hive/data/genomes/hg38/chrom.sizes
HS1_SIZES=/hive/data/genomes/hs1/chrom.sizes

for db in hg38 hs1; do
    if [[ "$db" == "hg38" ]]; then
        vcf="$HG38_VCF"; sizes="$HG38_SIZES"
    else
        vcf="$HS1_VCF";  sizes="$HS1_SIZES"
    fi
    echo "[$(date +%T)] $db: VCF -> BED..."
    python3 "$CONV" "$vcf" "sv.$db.bed" "$sizes"

    echo "[$(date +%T)] $db: sorting..."
    bedSort "sv.$db.bed" "sv.$db.sorted.bed"

    echo "[$(date +%T)] $db: bedToBigBed..."
    bedToBigBed -type=bed9+ -tab -as="$AS" \
        "sv.$db.sorted.bed" "$sizes" "sv.$db.bb"

    rm -f "sv.$db.bed" "sv.$db.sorted.bed"
    ls -lh "sv.$db.bb"
done

echo "[$(date +%T)] done."

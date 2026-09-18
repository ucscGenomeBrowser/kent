#!/bin/bash
# Build both MaveMD tracks for hg38 from a fetchMaveMd.py download.
#
#   runBuild.sh <downloadDir> <buildDir>
#
# Produces, in <buildDir>: mavemdVar.bb (one item per variant per score set) and
# mavemdMap.bb (one variant effect map per score set), plus mavemdFilters.ra, the
# generated trackDb filterValues block, and a log per stage.
set -e
set -o pipefail
export PATH=$PATH:$HOME/bin/x86_64

DOWNLOAD=${1:?usage: runBuild.sh <downloadDir> <buildDir>}
BUILD=${2:?usage: runBuild.sh <downloadDir> <buildDir>}
SCRIPTS=$(cd "$(dirname "$0")" && pwd)
CHROMSIZES=/hive/data/genomes/hg38/chrom.sizes

mkdir -p "$BUILD"
cd "$BUILD"

echo "[$(date +%T)] 1. per-variant bed"
"$SCRIPTS/makeMaveMdVariants.py" "$DOWNLOAD" mavemdVar.bed \
    --raFragment mavemdFilters.ra --workDir . 2> buildVar.log
tail -n 14 buildVar.log

echo "[$(date +%T)] 2. heatmap bed"
"$SCRIPTS/makeMaveMdHeatmap.py" "$DOWNLOAD" mavemdMap.bed 2> buildMap.log
tail -n 9 buildMap.log

echo "[$(date +%T)] 3. sort"
bedSort mavemdVar.bed mavemdVar.sorted.bed
bedSort mavemdMap.bed mavemdMap.sorted.bed

echo "[$(date +%T)] 4. bigBed"
bedToBigBed -type=bed12+34 -tab -as="$SCRIPTS/mavemdVariants.as" \
    -extraIndex=name,clinGenId,variantUrn \
    mavemdVar.sorted.bed "$CHROMSIZES" mavemdVar.bb 2>&1 | tail -2
bedToBigBed -type=bed12+20 -tab -as="$SCRIPTS/mavemdHeatmap.as" \
    mavemdMap.sorted.bed "$CHROMSIZES" mavemdMap.bb 2>&1 | tail -2

echo "[$(date +%T)] done"
bigBedInfo mavemdVar.bb | egrep 'itemCount|basesCovered|fieldCount'
bigBedInfo mavemdMap.bb | egrep 'itemCount|basesCovered|fieldCount'

#!/bin/bash
# buildTestHub.sh - regenerate the Mode C reference hub data.
#
# Builds a self-contained hs1 hub with two pairs of stanzas:
#   * modeC_bb_native / modeC_bb_lifted -- bigBed12, N features in chr22.
#   * modeC_bw_native / modeC_bw_lifted -- bigWig, one signal value per BW_STEP
#     bp across the same chr22 region.
# Each pair shares the same underlying data; the lifted variant points at an
# hg38-coord file plus quickLiftUrl. Drops everything (bb/bw + chain pair +
# hub.txt) into $1 (default ~/public_html/quickLiftBench/testHub).
#
# Refs Redmine #37445.

set -euo pipefail

N="${N:-5000}"
SEED="${SEED:-42}"
REGION_START="${REGION_START:-15000000}"
REGION_END="${REGION_END:-50000000}"
BW_STEP="${BW_STEP:-1000}"
DEST="${1:-$HOME/public_html/quickLiftBench/testHub}"

HG38_CHROM_SIZES=/hive/data/genomes/hg38/chrom.sizes
HS1_CHROM_SIZES=/hive/data/genomes/hs1/chrom.sizes
HG38_TO_HS1_CHAIN=/gbdb/hg38/liftOver/hg38ToHs1.over.chain.gz

# bigChain pair used by quickLift for hg38 -> hs1 on-the-fly remap.
# Copied into the hub dir so the bench is self-contained on one host.
QUICK_CHAIN_SRC=/hive/data/genomes/hs1/bed/lastz.hg38/axtChain/hs1.hg38.quick.bb
QUICK_CHAIN_LINK_SRC=/hive/data/genomes/hs1/bed/lastz.hg38/axtChain/hs1.hg38.quickLink.bb

mkdir -p "$DEST"
cd "$DEST"

python3 - "$N" "$SEED" "$REGION_START" "$REGION_END" > modeC_hg38.bed <<'PY'
import random, sys
N, seed, start_region, end_region = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
random.seed(seed)
chrom = "chr22"
span = end_region - start_region
# Lay features at regular stride so they don't overlap (~5kb each).
stride = max(span // N, 6000)
for i in range(N):
    s = start_region + i * stride
    e = s + 5000
    if e > end_region:
        break
    strand = "+" if i % 2 == 0 else "-"
    thickStart, thickEnd = s + 1000, e - 1000
    print("\t".join(map(str, [
        chrom, s, e, f"feat_{i:05d}", 0, strand,
        thickStart, thickEnd, "100,100,200",
        3, "500,2000,500,", "0,1500,4500,"
    ])))
PY

liftOver -bedPlus=12 modeC_hg38.bed "$HG38_TO_HS1_CHAIN" modeC_hs1.bed unmapped_hs1.bed

sort -k1,1 -k2,2n modeC_hg38.bed -o modeC_hg38.bed
sort -k1,1 -k2,2n modeC_hs1.bed  -o modeC_hs1.bed

bedToBigBed -type=bed12 modeC_hg38.bed "$HG38_CHROM_SIZES" modeC_hg38.bb
bedToBigBed -type=bed12 modeC_hs1.bed  "$HS1_CHROM_SIZES"  modeC_hs1.bb

# ---- bigWig: BedGraph at BW_STEP bp resolution, lifted by chain ----
python3 - "$REGION_START" "$REGION_END" "$BW_STEP" <<'PY' > modeC_hg38.bedGraph
import math, sys
s0, e0, step = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
chrom = "chr22"
for s in range(s0, e0, step):
    e = s + step
    # smooth signal in [50, 150]; chrom-position-driven, deterministic
    v = 100 + 50 * math.sin((s - s0) / 100000.0)
    print(f"{chrom}\t{s}\t{e}\t{v:.4f}")
PY

# liftOver of bedGraph: keep 4th column (value) as bedPlus extra.
liftOver -bedPlus=3 modeC_hg38.bedGraph "$HG38_TO_HS1_CHAIN" \
    modeC_hs1.bedGraph unmapped_hs1.bedGraph

# liftOver may emit overlapping intervals when a chain block edge falls
# inside a step; bedGraphToBigWig requires strictly non-overlapping. Sort,
# then drop any line whose start is < the previous line's end (cheap
# defensive trim; loses at most a handful of bins per chain edge).
sort -k1,1 -k2,2n modeC_hg38.bedGraph -o modeC_hg38.bedGraph
sort -k1,1 -k2,2n modeC_hs1.bedGraph  | awk 'BEGIN{OFS="\t"; pc=""; pe=-1} {if (!($1==pc && $2<pe)) {print; pc=$1; pe=$3}}' > modeC_hs1.bedGraph.trim
mv modeC_hs1.bedGraph.trim modeC_hs1.bedGraph

bedGraphToBigWig modeC_hg38.bedGraph "$HG38_CHROM_SIZES" modeC_hg38.bw
bedGraphToBigWig modeC_hs1.bedGraph  "$HS1_CHROM_SIZES"  modeC_hs1.bw

# Co-locate the quickLift chain pair so both stanzas fetch from one host.
# quickLift's chain loader constructs the link filename from the .bb name
# by replacing ".bb" with ".link.bb", so the link file MUST be named
# <chain>.link.bb (not <chain>Link.bb).
cp "$QUICK_CHAIN_SRC"      hg38ToHs1.quick.bb
cp "$QUICK_CHAIN_LINK_SRC" hg38ToHs1.quick.link.bb

# hub.txt (useOneFile -- single self-contained file)
cat > hub.txt <<EOF
hub quickLiftBench_modeC
shortLabel quickLiftBench Mode C
longLabel quickLiftBench Mode C: native vs quickLifted view of the same synthetic data
useOneFile on
email braney@ucsc.edu
descriptionUrl https://redmine.gi.ucsc.edu/issues/37445

genome hs1

track modeC_bb_native
shortLabel modeC bb native
longLabel Mode C bigBed native: BED12 features in hs1 coords
type bigBed 12
bigDataUrl modeC_hs1.bb
visibility pack
color 0,128,0
priority 1

track modeC_bb_lifted
shortLabel modeC bb lifted
longLabel Mode C bigBed lifted: same features in hg38 coords + quickLift hg38->hs1
type bigBed 12
bigDataUrl modeC_hg38.bb
quickLiftUrl hg38ToHs1.quick.bb
quickLiftDb hg38
visibility pack
color 0,128,0
priority 2

track modeC_bw_native
shortLabel modeC bw native
longLabel Mode C bigWig native: signal in hs1 coords
type bigWig
bigDataUrl modeC_hs1.bw
visibility full
color 0,128,0
viewLimits 0:200
priority 3

track modeC_bw_lifted
shortLabel modeC bw lifted
longLabel Mode C bigWig lifted: same signal in hg38 coords + quickLift hg38->hs1
type bigWig
bigDataUrl modeC_hg38.bw
quickLiftUrl hg38ToHs1.quick.bb
quickLiftDb hg38
visibility full
color 0,128,0
viewLimits 0:200
priority 4
EOF

echo "hub built at $DEST"
echo "  modeC_hg38.bb: $(wc -l < modeC_hg38.bed) features"
echo "  modeC_hs1.bb:  $(wc -l < modeC_hs1.bed) features (after liftOver)"
echo "  bedGraph bins: hg38=$(wc -l < modeC_hg38.bedGraph) hs1=$(wc -l < modeC_hs1.bedGraph)"

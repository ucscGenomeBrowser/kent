#!/bin/bash
# Build mouse Dixon 2012 TAD domains (tadsDixon) for mm10 + mm39 by liftOver from the
# native mm9 calls. refs #21599
# Source: Dixon 2012 Nature suppl Table S3 "Combined" sheets, mouse mESC (2200) + cortex
# (1518) domains, mm9, 40 kb (extracted earlier to lifttest/mm9_{mesc,cortex}_domains.bed).
set -beEu -o pipefail

SRC=/hive/users/lrnassar/claude/RM21599/lifttest
AS=/hive/data/outside/tad/tadDomain.as
declare -A CHAIN=( [mm10]=/gbdb/mm9/liftOver/mm9ToMm10.over.chain.gz \
                   [mm39]=/gbdb/mm9/liftOver/mm9ToMm39.over.chain.gz )
declare -A CS=( [mm10]=/hive/data/genomes/mm10/chrom.sizes \
                [mm39]=/hive/data/genomes/mm39/chrom.sizes )
# cell type -> (source bed, display name, bigBed basename)
SRCBED_mesc=mm9_mesc_domains.bed;   NAME_mesc=mESC;   BB_mesc=tadsDixonMESC
SRCBED_cortex=mm9_cortex_domains.bed; NAME_cortex=Cortex; BB_cortex=tadsDixonCortex

tmp=$(mktemp -d)
for asm in mm10 mm39; do
    out=/hive/data/outside/tad/dixon2012/build/$asm
    mkdir -p "$out"
    for ct in mesc cortex; do
        eval src=\$SRCBED_$ct; eval name=\$NAME_$ct; eval bb=\$BB_$ct
        # set col4 = cell-type name (matches human Dixon: mouseOver "Cell type: $name")
        awk -v n="$name" 'BEGIN{OFS="\t"}{print $1,$2,$3,n}' "$SRC/$src" > "$tmp/in.bed"
        liftOver -bedPlus=4 -tab "$tmp/in.bed" "${CHAIN[$asm]}" "$tmp/lift.bed" "$tmp/unmapped" || true
        nin=$(wc -l < "$tmp/in.bed"); nout=$(wc -l < "$tmp/lift.bed")
        echo "$name -> $asm: $nin -> $nout ($((nin-nout)) dropped)"
        # primary chroms only (drop *_random/_alt), clip to bounds, sort
        awk '$1 !~ /_/' "$tmp/lift.bed" | bedClip stdin "${CS[$asm]}" "$tmp/clip.bed"
        sort -k1,1 -k2,2n "$tmp/clip.bed" > "$tmp/sort.bed"
        bedToBigBed -type=bed4 -tab -as="$AS" "$tmp/sort.bed" "${CS[$asm]}" "$out/$bb.bb"
    done
done
rm -rf "$tmp"
echo "DONE: $(ls /hive/data/outside/tad/dixon2012/build/mm10/*.bb /hive/data/outside/tad/dixon2012/build/mm39/*.bb)"

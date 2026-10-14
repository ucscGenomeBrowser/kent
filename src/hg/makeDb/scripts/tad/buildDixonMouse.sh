#!/bin/bash
# Build mouse Dixon 2012 TAD domains (tadsDixon) for mm10 + mm39 by liftOver from the
# native mm9 calls. refs #21599
# Source: Dixon 2012 Nature suppl Table S3 "Combined" sheets, mouse mESC (2200) + cortex
# (1518) domains, mm9, 40 kb (extracted by extractTadsSupplements.py into
# <tadDir>/dixon2012/source/mm9_{mesc,cortex}_domains.bed).
# usage: buildDixonMouse.sh [tadDir]   (default /hive/data/outside/tad)
# Lifted items are kept on primary chroms only, trimmed to the chrom end, and dropped when they
# come out more than twice their mm9 size (the chain jumped a gap or rearranged block).
set -beEu -o pipefail

tad=${1:-/hive/data/outside/tad}
SRC=$tad/dixon2012/source
AS=$(cd "$(dirname "$0")" && pwd)/tadDomain.as
declare -A CHAIN=( [mm10]=/gbdb/mm9/liftOver/mm9ToMm10.over.chain.gz \
                   [mm39]=/gbdb/mm9/liftOver/mm9ToMm39.over.chain.gz )
declare -A CS=( [mm10]=/hive/data/genomes/mm10/chrom.sizes \
                [mm39]=/hive/data/genomes/mm39/chrom.sizes )
# cell type -> (source bed, display name, bigBed basename)
SRCBED_mesc=mm9_mesc_domains.bed;   NAME_mesc=mESC;   BB_mesc=tadsDixonMESC
SRCBED_cortex=mm9_cortex_domains.bed; NAME_cortex=Cortex; BB_cortex=tadsDixonCortex

tmp=$(mktemp -d /data/tmp/buildDixonMouse.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
log=$tad/dixon2012/build/buildDixonMouse.log; mkdir -p $tad/dixon2012/build; : > "$log"
for asm in mm10 mm39; do
    out=$tad/dixon2012/build/$asm
    mkdir -p "$out"
    for ct in mesc cortex; do
        eval src=\$SRCBED_$ct; eval name=\$NAME_$ct; eval bb=\$BB_$ct
        # col4 = cell-type name (matches human Dixon: mouseOver "Cell type: $name");
        # col5 = mm9 size, carried through the lift for the size check, then removed
        awk -v n="$name" 'BEGIN{OFS="\t"}{print $1,$2,$3,n,$3-$2}' "$SRC/$src" > "$tmp/in.bed"
        liftOver -bedPlus=4 -tab "$tmp/in.bed" "${CHAIN[$asm]}" "$tmp/lift.bed" "$tmp/unmapped"
        awk -F'\t' -v OFS='\t' -v cnt="$tmp/counts" -v infl="$out/$bb.inflated" 'NR==FNR{sz[$1]=$2; next}
            $1 ~ /_/ {nonPrim++; next}
            {if ($3 > sz[$1]) {$3 = sz[$1]; trimmed++}
             if ($3 - $2 > 2 * $NF) {inflated++; print > infl; next}
             NF--; print}
            END{printf "%d\t%d\t%d\n", nonPrim, trimmed, inflated > cnt}' \
            "${CS[$asm]}" "$tmp/lift.bed" | sort -k1,1 -k2,2n > "$tmp/sort.bed"
        read nonPrim trimmed inflated < "$tmp/counts"
        bedToBigBed -type=bed4 -tab -as="$AS" "$tmp/sort.bed" "${CS[$asm]}" "$out/$bb.bb"
        printf "%s\t%s\tin %d\tunmapped %d\tnonPrimary %d\ttrimmed %d\tinflated %d\tfinal %d\n" \
            "$name" "$asm" "$(wc -l < "$tmp/in.bed")" "$(grep -vc '^#' "$tmp/unmapped" || true)" \
            "$nonPrim" "$trimmed" "$inflated" "$(wc -l < "$tmp/sort.bed")" | tee -a "$log"
    done
done

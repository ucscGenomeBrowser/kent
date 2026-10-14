#!/bin/bash
# Lift the native-mm10 3DGB faceted TAD track to mm39. refs #21599
# Lifts the finished mm10 bed4 bigBeds, copies the metadata TSV, and transforms the mm10 stanza
# -> mm39 (gbdb path + "lifted from mm10" longLabels).
# usage: liftTads3dgbMouse.sh [tadDir]   (default /hive/data/outside/tad)
# Lifted items are kept on primary chroms only, trimmed to the chrom end, and dropped when they
# come out more than twice their mm10 size (the chain jumped a gap or rearranged block).
set -beEu -o pipefail
tad=${1:-/hive/data/outside/tad}
SRC=$tad/3dgenome/build/mm10
DST=$tad/3dgenome/build/mm39
CHAIN=/gbdb/mm10/liftOver/mm10ToMm39.over.chain.gz
CS=/hive/data/genomes/mm39/chrom.sizes
AS=$(cd "$(dirname "$0")" && pwd)/tadDomain.as
mkdir -p "$DST/tads3dgb"
tmp=$(mktemp -d /data/tmp/liftTads3dgbMouse.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
log=$DST/liftTads3dgbMouse.log; : > "$log"
for bb in "$SRC"/tads3dgb/*.bb; do
    name=$(basename "$bb")
    # column 5 = mm10 size, carried through the lift for the size check, then removed
    bigBedToBed "$bb" stdout | awk -F'\t' -v OFS='\t' '{print $0, $3-$2}' > "$tmp/in.bed"
    liftOver -bedPlus=4 -tab "$tmp/in.bed" "$CHAIN" "$tmp/lift.bed" "$tmp/unmapped"
    awk -F'\t' -v OFS='\t' -v cnt="$tmp/counts" -v infl="$DST/${name%.bb}.inflated" 'NR==FNR{sz[$1]=$2; next}
        $1 ~ /_/ {nonPrim++; next}
        {if ($3 > sz[$1]) {$3 = sz[$1]; trimmed++}
         if ($3 - $2 > 2 * $NF) {inflated++; print > infl; next}
         NF--; print}
        END{printf "%d\t%d\t%d\n", nonPrim, trimmed, inflated > cnt}' \
        "$CS" "$tmp/lift.bed" | sort -k1,1 -k2,2n > "$tmp/sort.bed"
    read nonPrim trimmed inflated < "$tmp/counts"
    bedToBigBed -type=bed4 -tab -as="$AS" "$tmp/sort.bed" "$CS" "$DST/tads3dgb/$name"
    printf "%s\tin %d\tunmapped %d\tnonPrimary %d\ttrimmed %d\tinflated %d\tfinal %d\n" \
        "$name" "$(wc -l < "$tmp/in.bed")" "$(grep -vc '^#' "$tmp/unmapped" || true)" \
        "$nonPrim" "$trimmed" "$inflated" "$(wc -l < "$tmp/sort.bed")" | tee -a "$log"
done
cp "$SRC/tads3dgb_metadata.tsv" "$DST/tads3dgb_metadata.tsv"
sed -e 's#/gbdb/mm10/#/gbdb/mm39/#g' \
    -e 's#(3D Genome Browser 2.0)#(3DGB 2.0, lifted from mm10)#' \
    -e '/^    longLabel /s#)$#, lifted from mm10)#' \
    "$SRC/tads3dgb.ra" > "$DST/tads3dgb.ra"
echo "DONE -> $DST/ ($(ls $DST/tads3dgb/*.bb | wc -l) bigBeds)"

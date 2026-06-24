#!/bin/bash
# Lift the native-mm10 3DGB faceted TAD track to mm39. refs #21599
# Lifts the finished mm10 bed4 bigBeds, copies the metadata TSV, and transforms the mm10 stanza
# -> mm39 (gbdb path + "lifted from mm10" longLabels).
set -beEu -o pipefail
SRC=/hive/data/outside/tad/3dgenome/build/mm10
DST=/hive/data/outside/tad/3dgenome/build/mm39
CHAIN=/gbdb/mm10/liftOver/mm10ToMm39.over.chain.gz
CS=/hive/data/genomes/mm39/chrom.sizes
AS=/hive/data/outside/tad/tadDomain.as
mkdir -p "$DST/tads3dgb"
tmp=$(mktemp -d)
for bb in "$SRC"/tads3dgb/*.bb; do
    name=$(basename "$bb")
    bigBedToBed "$bb" "$tmp/in.bed"
    liftOver -bedPlus=4 -tab "$tmp/in.bed" "$CHAIN" "$tmp/lift.bed" "$tmp/unmapped" || true
    awk '$1 !~ /_/' "$tmp/lift.bed" | bedClip stdin "$CS" "$tmp/clip.bed"
    sort -k1,1 -k2,2n "$tmp/clip.bed" > "$tmp/sort.bed"
    bedToBigBed -type=bed4 -tab -as="$AS" "$tmp/sort.bed" "$CS" "$DST/tads3dgb/$name"
done
rm -rf "$tmp"
cp "$SRC/tads3dgb_metadata.tsv" "$DST/tads3dgb_metadata.tsv"
sed -e 's#/gbdb/mm10/#/gbdb/mm39/#g' \
    -e 's#(3D Genome Browser 2.0)#(3DGB 2.0, lifted from mm10)#' \
    -e '/^    longLabel /s#)$#, lifted from mm10)#' \
    "$SRC/tads3dgb.ra" > "$DST/tads3dgb.ra"
echo "DONE -> $DST/ ($(ls $DST/tads3dgb/*.bb | wc -l) bigBeds)"

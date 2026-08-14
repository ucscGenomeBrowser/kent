#!/bin/bash
# Lift the native-mm10 ENCODE faceted TAD track to mm39. refs #21599
# Lifts the finished mm10 bigBeds (bed4+5; -bedPlus=4 -tab carries the 5 Arrowhead score cols),
# copies the (assembly-independent) metadata TSV, and transforms the mm10 stanza -> mm39
# (gbdb path + "lifted from mm10" in the longLabels). The Calls facet stays "Arrowhead (mm10)"
# to record the native call assembly.
set -beEu -o pipefail
SRC=/hive/data/outside/tad/encode/build/mm10
DST=/hive/data/outside/tad/encode/build/mm39
CHAIN=/gbdb/mm10/liftOver/mm10ToMm39.over.chain.gz
CS=/hive/data/genomes/mm39/chrom.sizes
AS=/hive/data/outside/tad/tadDomainEncode.as
mkdir -p "$DST/tadsEncode"
tmp=$(mktemp -d)
for bb in "$SRC"/tadsEncode/*.bb; do
    name=$(basename "$bb")
    bigBedToBed "$bb" "$tmp/in.bed"
    liftOver -bedPlus=4 -tab "$tmp/in.bed" "$CHAIN" "$tmp/lift.bed" "$tmp/unmapped" || true
    nin=$(wc -l < "$tmp/in.bed"); nout=$(wc -l < "$tmp/lift.bed")
    awk '$1 !~ /_/' "$tmp/lift.bed" | bedClip stdin "$CS" "$tmp/clip.bed"
    sort -k1,1 -k2,2n "$tmp/clip.bed" > "$tmp/sort.bed"
    bedToBigBed -type=bed4+5 -tab -as="$AS" "$tmp/sort.bed" "$CS" "$DST/tadsEncode/$name"
    echo "  $name: $nin -> $nout"
done
rm -rf "$tmp"
cp "$SRC/tadsEncode_metadata.tsv" "$DST/tadsEncode_metadata.tsv"
sed -e 's#/gbdb/mm10/#/gbdb/mm39/#g' \
    -e 's# (Arrowhead/Hi-C)$#, lifted from mm10#' \
    -e '/^    longLabel ENCODE TADs in /s#)$#, lifted from mm10)#' \
    "$SRC/tadsEncode.ra" > "$DST/tadsEncode.ra"
echo "DONE -> $DST/ ($(ls $DST/tadsEncode/*.bb | wc -l) bigBeds)"

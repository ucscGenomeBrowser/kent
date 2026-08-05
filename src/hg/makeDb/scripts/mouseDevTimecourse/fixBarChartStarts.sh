#!/bin/bash
# Correct a 1-bp off-by-one in the chromStart of the Wold Lab mouse development
# bigBarChart files.
#
# The hub's builder wrote 1-based GTF gene starts into the 0-based BED
# chromStart field, so every gene sits one base to the right of its true start
# while chromEnd is correct. Verified against GENCODE VM21 and VM4 on mm10:
# 42081/42093 (M21) and 35323/35333 (M4) genes were start+1, none exact, with
# ends matching. mm39 inherits the offset through the liftOver. See #37001.
#
# Reported upstream to Diane Trout; rerun this after any hub refetch, in the
# same way the tissue reorder and color update have to be reapplied.
#
# Usage: fixBarChartStarts.sh <db> <file.bb> [<file.bb> ...]
# Originals are kept alongside as <name>.bb.preStartFix

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "usage: fixBarChartStarts.sh <db> <file.bb> [<file.bb> ...]" >&2
    exit 1
fi

DB=$1; shift
SIZES=/hive/data/genomes/$DB/chrom.sizes
[ -s "$SIZES" ] || { echo "no chrom.sizes for $DB at $SIZES" >&2; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

for BB in "$@"; do
    [ -s "$BB" ] || { echo "missing: $BB" >&2; exit 1; }
    NAME=$(basename "$BB")
    echo "=== $NAME ==="

    # Reuse the file's own schema so the rebuilt file is otherwise identical.
    bigBedInfo -as "$BB" | sed -n '/^table /,/^    )/p' > "$TMP/schema.as"
    [ -s "$TMP/schema.as" ] || { echo "could not extract .as from $NAME" >&2; exit 1; }

    bigBedToBed "$BB" "$TMP/in.bed"
    IN_ROWS=$(wc -l < "$TMP/in.bed")

    # Refuse to run on a file that has already been corrected, which would
    # underflow chromStart to -1. mt-Tf legitimately sits at chrM:0 after the
    # fix, so this is the signature of a rerun. Checked in its own pass: awk's
    # exit status is not observable from the middle of a pipeline.
    if awk -F'\t' '$2 <= 0 {print; found=1} END {exit !found}' "$TMP/in.bed" \
         > "$TMP/zero.bed"; then
        echo "$NAME already has chromStart <= 0 (e.g. $(head -1 "$TMP/zero.bed" \
            | cut -f1-4 | tr '\t' ' ')) - already corrected, refusing to shift again" >&2
        exit 1
    fi

    awk -F'\t' 'BEGIN{OFS="\t"} {$2 = $2 - 1; print}' "$TMP/in.bed" \
      | LC_ALL=C sort -k1,1 -k2,2n > "$TMP/out.bed"

    OUT_ROWS=$(wc -l < "$TMP/out.bed")
    [ "$IN_ROWS" = "$OUT_ROWS" ] || {
        echo "row count changed: $IN_ROWS -> $OUT_ROWS" >&2; exit 1; }

    bedToBigBed -as="$TMP/schema.as" -type=bed6+3 \
        "$TMP/out.bed" "$SIZES" "$TMP/new.bb"

    NEW_ROWS=$(bigBedInfo "$TMP/new.bb" | awk '/^itemCount/{gsub(",","",$2); print $2}')
    [ "$NEW_ROWS" = "$OUT_ROWS" ] || {
        echo "itemCount mismatch: expected $OUT_ROWS got $NEW_ROWS" >&2; exit 1; }

    cp -p "$BB" "$BB.preStartFix"
    mv "$TMP/new.bb" "$BB"
    chmod 664 "$BB"
    echo "  $IN_ROWS items, starts shifted -1, original kept at $NAME.preStartFix"
    rm -f "$TMP/in.bed" "$TMP/out.bed"
done

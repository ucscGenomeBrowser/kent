#!/bin/bash
# Rewrite an already-built segdups or pclai bigBed in place (no re-download):
# blank the (over-long) name field and move the useful values into named fields
# so they can be shown on mouseover. Usage: hprc2annotFixBed.sh TRACK BBFILE
set -u -o pipefail
track=$1; bb=$2
SCR=$HOME/kent/src/hg/makeDb/scripts/hprc2annot
tmp=$(mktemp -d "${TMPDIR:-/data/tmp}/fixbed.XXXXXX"); trap 'rm -rf "$tmp"' EXIT

# chrom.sizes straight from the bigBed header (PanSN names, no assembly lookup)
bigBedInfo -chroms "$bb" | awk 'NF==3 && $2~/^[0-9]+$/ && $3~/^[0-9]+$/{print $1"\t"$3}' > "$tmp/sizes"
[ -s "$tmp/sizes" ] || { echo "NO_SIZES $bb" >&2; exit 3; }
bigBedToBed "$bb" "$tmp/in.bed" 2>/dev/null

# This conversion is NOT idempotent: run twice on pclai it re-parses an already
# parsed name (which is now blank), blanking window and pca and shifting the
# segment PCA into the wrong column. Refuse to touch a file that is already in
# the converted layout. segdups gains no columns, so check the blank name there.
fields=$(awk -F'\t' 'NR==1{print NF; exit}' "$tmp/in.bed")
name1=$(awk -F'\t' 'NR==1{print $4; exit}' "$tmp/in.bed")
case "$track" in
  pclai)   [ "$fields" = 12 ] && { echo "ALREADY_CONVERTED pclai $bb"; exit 0; } ;;
  segdups) [ "$fields" = 15 ] && [ -z "$name1" ] && { echo "ALREADY_CONVERTED segdups $bb"; exit 0; } ;;
esac

case "$track" in
  segdups)
    # existing bed9+6 (name=partner); just blank the name column
    awk -F'\t' 'BEGIN{OFS="\t"}{$4=""; print}' "$tmp/in.bed" \
      | LC_COLLATE=C sort -k1,1 -k2,2n > "$tmp/out.bed"
    type=bed9+6; as=$SCR/segdups.as ;;
  pclai)
    # existing bed9+1 (name="SAMPLE/hN/<window>_(PC1,PC2)", col10=segment PCA);
    # -> bed9+3: blank name; window, pca (this window), pcaSegment (=old col10)
    awk -F'\t' 'BEGIN{OFS="\t"}
      { seg=$4; sub(/^[^/]*\/[^/]*\//,"",seg); k=split(seg,b,"_"); pca=b[k];
        win=(pca!="")?substr(seg,1,length(seg)-length(pca)-1):seg;
        print $1,$2,$3,"",$5,$6,$7,$8,$9,win,pca,$10 }' "$tmp/in.bed" \
      | LC_COLLATE=C sort -k1,1 -k2,2n > "$tmp/out.bed"
    type=bed9+3; as=$SCR/pclai.as ;;
  *) echo "UNKNOWN_TRACK $track" >&2; exit 2 ;;
esac

bedToBigBed -type=$type -tab -as="$as" "$tmp/out.bed" "$tmp/sizes" "$tmp/new.bb" 2>"$tmp/err" \
  || { echo "BB_FAIL $track $bb" >&2; cat "$tmp/err" >&2; exit 6; }
mv "$tmp/new.bb" "$bb"
echo "OK $track $bb"

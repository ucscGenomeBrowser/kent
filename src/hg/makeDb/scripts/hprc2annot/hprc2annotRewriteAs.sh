#!/bin/bash
# Rewrite an already-built bigBed in place with the current .as, without
# re-downloading anything. The column layout is not touched: this is only for
# picking up edits to field names and field descriptions in the .as, which are
# stored inside the bigBed and so cannot be changed by editing the .as alone.
#
# Unlike hprc2annotFixBed.sh this is safe to run repeatedly -- it re-emits the
# same columns -- but it will refuse a file whose column count does not match the
# .as, which is the signal that the layout, not just the labels, has changed and
# that a real rebuild (hprc2annotBuild.sh) is what is needed.
#
# Usage: hprc2annotRewriteAs.sh TRACK BBFILE
set -u -o pipefail
track=$1; bb=$2
SCR=$HOME/kent/src/hg/makeDb/scripts/hprc2annot

case "$track" in
  pclai)   as=$SCR/pclai.as;   type=bed9+3; want=12 ;;
  segdups) as=$SCR/segdups.as; type=bed9+6; want=15 ;;
  *) echo "UNKNOWN_TRACK $track" >&2; exit 2 ;;
esac
[ -s "$as" ] || { echo "NO_AS $as" >&2; exit 2; }

tmp=$(mktemp -d "${TMPDIR:-/data/tmp}/rewriteAs.XXXXXX"); trap 'rm -rf "$tmp"' EXIT

# chrom.sizes straight from the bigBed header (PanSN names, no assembly lookup)
bigBedInfo -chroms "$bb" | awk 'NF==3 && $2~/^[0-9]+$/ && $3~/^[0-9]+$/{print $1"\t"$3}' > "$tmp/sizes"
[ -s "$tmp/sizes" ] || { echo "NO_SIZES $bb" >&2; exit 3; }

bigBedToBed "$bb" "$tmp/in.bed" 2>/dev/null
inCount=$(wc -l < "$tmp/in.bed")
fields=$(awk -F'\t' 'NR==1{print NF; exit}' "$tmp/in.bed")
[ "$fields" = "$want" ] || { echo "WRONG_LAYOUT $track $bb has $fields fields, .as wants $want" >&2; exit 4; }

# bigBedToBed already emits in chrom order, but sort anyway so the input is
# unambiguously valid for bedToBigBed.
LC_COLLATE=C sort -k1,1 -k2,2n "$tmp/in.bed" > "$tmp/out.bed"
bedToBigBed -type=$type -tab -as="$as" "$tmp/out.bed" "$tmp/sizes" "$tmp/new.bb" 2>"$tmp/err" \
  || { echo "BB_FAIL $track $bb" >&2; cat "$tmp/err" >&2; exit 6; }

outCount=$(bigBedInfo "$tmp/new.bb" | awk '/^itemCount:/{gsub(/,/,"",$2); print $2}')
[ "$inCount" = "$outCount" ] || { echo "COUNT_MISMATCH $bb $inCount -> $outCount" >&2; exit 7; }

mv "$tmp/new.bb" "$bb"
echo "OK $track $bb $outCount"

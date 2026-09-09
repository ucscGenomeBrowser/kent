#!/bin/bash
# Convert one pcLAI GRCh38-coordinate BED into a UCSC bigBed.
#
# The source is a bed9+1: the name column packs three things
# ("SAMPLE/hN/<window>_(PC1,PC2)") and column 10 is the source's "centroid"
# column, the discretized ancestry of the window written as the PCA centroid of
# that ancestry cluster (hence only four distinct values across the whole
# release). Those are split into their own named fields (haplotype, window, pca,
# centroid) so they can be shown on mouseover and fed to the details-page
# scatterplot, and the name column is blanked: the
# packed string is too long to draw as an item label and carries no extra
# information once the fields are split out.
#
# thickStart/thickEnd are rewritten to the item bounds. The source format
# specifies thickStart == chromStart and thickEnd == chromEnd; thickEnd always
# holds, and thickStart holds except in
# 54811 of the 11936603 windows (0.46%), where it is chromStart-1 and so outside
# the item -- an off-by-one in the projection that bedToBigBed rejects. The two
# columns carry no information for this annotation, so they are set to the item
# bounds rather than dropping those windows.
#
# Usage: hprcPclaiMakeBb.sh <in.bed> <out.bb> <chrom.sizes>
set -u -o pipefail
inBed=$1; outBb=$2; sizes=$3
SCR=$HOME/kent/src/hg/makeDb/scripts/hprcPclai
tmp=$(mktemp "${TMPDIR:-/data/tmp}/hprcPclai.XXXXXX.bed"); trap 'rm -f "$tmp"' EXIT

awk -F'\t' 'BEGIN{OFS="\t"}
  { n=split($4,a,"/");
    if (n!=3) { bad++; next }
    hap=a[1]"/"a[2]; rest=a[3];
    k=split(rest,b,"_"); pca=b[k];
    win=(pca!="") ? substr(rest,1,length(rest)-length(pca)-1) : rest;
    if ($7!=$2) fixedThick++;
    print $1,$2,$3,"",$5,$6,$7=$2,$8=$3,$9,hap,win,pca,$10 }
  END{ if (bad) print "UNPARSED_NAMES "bad" in '"$inBed"'" > "/dev/stderr";
       if (fixedThick) print "FIXED_THICKSTART "fixedThick" in '"$inBed"'" > "/dev/stderr" }' "$inBed" \
  | LC_COLLATE=C sort -k1,1 -k2,2n > "$tmp"

inCount=$(wc -l < "$inBed")
outCount=$(wc -l < "$tmp")
[ "$inCount" = "$outCount" ] || { echo "COUNT_MISMATCH $inBed $inCount -> $outCount" >&2; exit 4; }

bedToBigBed -type=bed9+4 -tab -as="$SCR/hprcPclai.as" "$tmp" "$sizes" "$outBb" \
  || { echo "BB_FAIL $inBed" >&2; exit 6; }
echo "OK $outBb $outCount"

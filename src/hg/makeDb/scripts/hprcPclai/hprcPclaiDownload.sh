#!/bin/bash
# Download the pcLAI GRCh38-coordinate BED files for all HPRC Release 2 haplotypes.
# The index CSV lists one s3:// path per haplotype; s3:// is rewritten to the
# public https endpoint of the same bucket. The submissions bucket resets
# connections under load, so downloads are retried and run only 8-wide.
# Usage: hprcPclaiDownload.sh <index.csv> <outDir>
set -u -o pipefail
idx=$1; outDir=$2
mkdir -p "$outDir"

fetchOne() {
    # args: sample haplotype s3path outDir
    local samp=$1 hap=$2 s3=$3 outDir=$4
    local out="$outDir/$samp.$hap.bed"
    [ -s "$out" ] && { echo "HAVE $samp.$hap"; return 0; }
    local url=${s3/s3:\/\/human-pangenomics\//https://s3-us-west-2.amazonaws.com/human-pangenomics/}
    curl -sfL --retry 8 --retry-delay 3 --retry-all-errors "$url" -o "$out.tmp" \
        || { echo "FAIL $samp.$hap $url" >&2; rm -f "$out.tmp"; return 1; }
    [ -s "$out.tmp" ] || { echo "EMPTY $samp.$hap $url" >&2; rm -f "$out.tmp"; return 1; }
    mv "$out.tmp" "$out"
    echo "OK $samp.$hap"
}
export -f fetchOne

# strip the CRLF the index CSV uses, skip the header
tail -n +2 "$idx" | tr -d '\r' \
  | awk -F, 'NF>=4 {print $1"\t"$2"\t"$4}' \
  | parallel --colsep '\t' -j 8 fetchOne {1} {2} {3} "$outDir"

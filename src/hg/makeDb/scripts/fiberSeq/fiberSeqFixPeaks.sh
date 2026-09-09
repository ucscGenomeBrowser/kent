#!/bin/bash
# Rewrite the FIRE peak bigBeds with a correct header.
#
# The files on the UW server carry narrowPeak data (10 columns: name, score,
# strand, signalValue, pValue, qValue and the point-source offset) and even
# embed the narrowPeak autoSql, but their bigBed header records a field count
# of 3.  bbFieldIndex() therefore cannot find signalValue or qValue, so no
# filter, no mouseOver and no details-page column works on them, and asking
# hgTracks for a signalValue filter aborts with "Field not found".
#
# This converts each file back to BED and rebuilds it as a bed6+4 bigBed with
# the narrowPeak schema, which changes no data, only the header.  Reported
# upstream; drop this step once the lab regenerates the files.
#
# Usage: fiberSeqFixPeaks.sh <dataDir> [jobs]

set -o pipefail

dataDir=$1
jobs=${2:-8}
if [ -z "$dataDir" ]; then
    echo "usage: $0 <dataDir> [jobs]" >&2
    exit 1
fi

scriptDir=$(dirname "$(readlink -f "$0")")
sampleList=$scriptDir/fiberSeqSamples.tsv
asFile=$(readlink -f ~/kent/src/hg/lib/encode/narrowPeak.as)
chromSizes=/hive/data/genomes/hg38/chrom.sizes

for f in "$asFile" "$chromSizes"; do
    [ -s "$f" ] || { echo "missing $f" >&2; exit 1; }
done

fixOne() {
    acc=$1; dataDir=$2; asFile=$3; chromSizes=$4
    src=$dataDir/$acc/fire-peaks.bb
    out=$dataDir/$acc/fire-peaks.ucsc.bb
    tmp=$dataDir/$acc/fire-peaks.$$.bed

    if [ ! -s "$src" ]; then
        echo "MISSING $acc/fire-peaks.bb" >&2
        return 1
    fi
    # Already converted and newer than the source: nothing to do.
    if [ -s "$out" ] && [ "$out" -nt "$src" ]; then
        echo "have $acc"
        return 0
    fi

    if ! bigBedToBed "$src" "$tmp"; then
        echo "FAIL $acc bigBedToBed" >&2
        rm -f "$tmp"
        return 1
    fi
    inRows=$(wc -l < "$tmp")
    # Every row must have the 10 narrowPeak columns; stop rather than let
    # bedToBigBed silently reinterpret a short row.
    ragged=$(awk -F'\t' 'NF!=10' "$tmp" | wc -l)
    if [ "$ragged" != "0" ]; then
        echo "FAIL $acc $ragged of $inRows rows are not 10 columns" >&2
        rm -f "$tmp"
        return 1
    fi
    # The peaks were called against the GRCh38 analysis set, which carries the
    # Epstein-Barr virus decoy.  chrEBV is not part of UCSC hg38, so those
    # peaks cannot be placed and are dropped here; the count is reported so it
    # is never a silent loss.  Any other unplaceable contig is an error.
    unknown=$(cut -f1 "$tmp" | sort -u | grep -v -x -F -f <(cut -f1 "$chromSizes" | sort -u) | grep -v -x chrEBV)
    if [ -n "$unknown" ]; then
        echo "FAIL $acc unexpected contigs: $(echo $unknown | tr '\n' ' ')" >&2
        rm -f "$tmp"
        return 1
    fi
    ebv=$(awk -F'\t' '$1=="chrEBV"' "$tmp" | wc -l)
    # signalValue and qValue come out of the pipeline with full double precision
    # (qValue values like 22.807437495448326), which bloats the file and reads
    # badly in a mouseover and on the details page.  Round both to 3 decimals.
    # pValue is the sentinel -1 throughout and is left exactly as it is.
    awk -F'\t' -v OFS='\t' '$1!="chrEBV" {
        $7 = sprintf("%.3f", $7);
        $9 = sprintf("%.3f", $9);
        print
    }' "$tmp" | sort -k1,1 -k2,2n > "$tmp.s" && mv "$tmp.s" "$tmp"
    if ! bedToBigBed -as="$asFile" -type=bed6+4 -tab "$tmp" "$chromSizes" "$out" 2>/dev/null; then
        echo "FAIL $acc bedToBigBed" >&2
        rm -f "$tmp" "$out"
        return 1
    fi
    outRows=$(bigBedInfo "$out" | awk -F': ' '$1=="itemCount"{gsub(",","",$2); print $2}')
    rm -f "$tmp"
    want=$((inRows - ebv))
    if [ "$want" != "$outRows" ]; then
        echo "FAIL $acc kept $outRows, expected $want (source $inRows, chrEBV $ebv)" >&2
        return 1
    fi
    echo "got $acc $outRows peaks (source $inRows, dropped $ebv on chrEBV)"
}
export -f fixOne

grep -v '^#' "$sampleList" | awk -F'\t' 'NF{print $1}' \
  | xargs -P "$jobs" -I{} bash -c 'fixOne "$1" "$2" "$3" "$4"' _ {} "$dataDir" "$asFile" "$chromSizes"

echo
echo "field counts after conversion (all should be 10):"
for f in "$dataDir"/*/fire-peaks.ucsc.bb; do
    bigBedInfo "$f" | awk -F': ' '$1=="fieldCount"{print $2}'
done | sort | uniq -c

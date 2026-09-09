#!/bin/bash
# Verify the mirrored Fiber-seq / CpG files: every expected file present, every
# one readable as a bigWig or bigBed, and report any that hold no data.  The
# empty-file report matters: at mirror time PM00001 (GM12878)
# hap1/hap2.percent.accessible.bw were 512-byte stubs on the server, i.e. valid
# bigWigs with zero bases covered, so a size check alone does not catch them.
#
# Usage: fiberSeqCheck.sh <outDir>

set -o pipefail

outDir=$1
if [ -z "$outDir" ]; then
    echo "usage: $0 <outDir>" >&2
    exit 1
fi

scriptDir=$(dirname "$(readlink -f "$0")")
sampleList=$scriptDir/fiberSeqSamples.tsv

bigWigs="all.percent.accessible.bw
hap1.percent.accessible.bw
hap2.percent.accessible.bw
cpg.combined.bw
cpg.hap1.bw
cpg.hap2.bw
cpg.diffs_all.bw
cpg.diffs_p0.01.bw
cpg.diffs_p0.001.bw
cpg.diffs_p0.0001.bw"
bigBeds="fire-peaks.bb
fire-peaks.ucsc.bb"

missing=0
broken=0
empty=0

grep -v '^#' "$sampleList" | while IFS=$'\t' read -r acc sample cellType hash; do
    [ -z "$acc" ] && continue
    for f in $bigWigs; do
        p=$outDir/$acc/$f
        if [ ! -s "$p" ]; then
            echo "MISSING $acc/$f"
            continue
        fi
        info=$(bigWigInfo "$p" 2>&1) || { echo "BROKEN $acc/$f"; continue; }
        bases=$(echo "$info" | awk -F': ' '$1=="basesCovered"{gsub(",","",$2); print $2}')
        # A placeholder bigWig is not necessarily basesCovered 0: the two bad
        # files on the server cover exactly one base with a value of zero.  The
        # smallest legitimate file here covers 4.1 million bases, so anything
        # under a thousand is a stub, not thin coverage.
        if [ "${bases:-0}" -lt 1000 ]; then
            echo "EMPTY $acc/$f (basesCovered ${bases:-0})"
        fi
    done
    for f in $bigBeds; do
        p=$outDir/$acc/$f
        if [ ! -s "$p" ]; then
            echo "MISSING $acc/$f"
            continue
        fi
        info=$(bigBedInfo "$p" 2>&1) || { echo "BROKEN $acc/$f"; continue; }
        n=$(echo "$info" | awk -F': ' '$1=="itemCount"{gsub(",","",$2); print $2}')
        if [ "${n:-0}" = "0" ]; then
            echo "EMPTY $acc/$f (itemCount 0)"
        fi
    done
done | tee "$outDir/fiberSeqCheck.log"

echo
echo "problems logged to $outDir/fiberSeqCheck.log"
echo "no lines above means all $(grep -vc '^#' "$sampleList") samples are complete and non-empty"

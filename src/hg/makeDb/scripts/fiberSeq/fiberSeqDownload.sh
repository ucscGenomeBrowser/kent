#!/bin/bash
# Mirror the Stergachis/Vollger lab Fiber-seq and CpG methylation bigWig/bigBed
# files for hg38 from the UW Kopah S3 server into the track data directory.
# The sample list and the per-sample S3 hash directories come from
# fiberSeqSamples.tsv, which was extracted from the lab's track hub
# (https://fiberseq.github.io/UCSC-Fiber-seq-hub/hg38/trackDb.txt) and from the
# per-sample CpG hubs that Shane Neph sent (cpg-hprc.txt).
#
# Usage: fiberSeqDownload.sh <outDir> [jobs]
#   outDir  where to write <accession>/<file>, e.g.
#           /hive/data/genomes/hg38/bed/fiberSeq
#   jobs    parallel downloads, default 8 (hgwdev budget allows up to 20)
#
# Re-running is safe: curl -C - resumes, and a file whose size already matches
# the server is skipped.

set -o pipefail

outDir=$1
jobs=${2:-8}
if [ -z "$outDir" ]; then
    echo "usage: $0 <outDir> [jobs]" >&2
    exit 1
fi

scriptDir=$(dirname "$(readlink -f "$0")")
sampleList=$scriptDir/fiberSeqSamples.tsv
s3Base=https://s3.kopah.uw.edu/userprod/web/public/hashed.PacBio-Fiber-seq

# The 11 files we mirror per sample.  The four cpg.diffs_* wiggles are the
# haplotype-difference significance thresholds shown as one overlay.
files="bw/all.percent.accessible.bw
bw/hap1.percent.accessible.bw
bw/hap2.percent.accessible.bw
bb/fire-peaks.bb
bw/cpg.combined.bw
bw/cpg.hap1.bw
bw/cpg.hap2.bw
bw/cpg.diffs_all.bw
bw/cpg.diffs_p0.01.bw
bw/cpg.diffs_p0.001.bw
bw/cpg.diffs_p0.0001.bw"

# remoteSize url -> byte count on the server, or empty if unreachable.
# A one-byte range request works where HEAD is unreliable on this Ceph gateway.
remoteSize() {
    curl -sS -r 0-0 -D - -o /dev/null "$1" 2>/dev/null \
        | tr -d '\r' | grep -i '^content-range:' | sed 's|.*/||'
}

fetchOne() {
    acc=$1; hash=$2; rel=$3; outDir=$4
    url=$s3Base/$acc/$hash/hg38/trackHub/$rel
    out=$outDir/$acc/$(basename "$rel")
    mkdir -p "$(dirname "$out")"

    want=$(remoteSize "$url")
    if [ -z "$want" ]; then
        echo "FAIL $acc $(basename "$rel") unreachable" >&2
        return 1
    fi
    have=$(stat -c %s "$out" 2>/dev/null || echo 0)
    if [ "$have" = "$want" ]; then
        echo "have $acc $(basename "$rel") $want"
        return 0
    fi
    if ! curl -sS -f -C - -o "$out" "$url"; then
        echo "FAIL $acc $(basename "$rel") download error" >&2
        return 1
    fi
    have=$(stat -c %s "$out" 2>/dev/null || echo 0)
    if [ "$have" != "$want" ]; then
        echo "FAIL $acc $(basename "$rel") got $have want $want" >&2
        return 1
    fi
    echo "got $acc $(basename "$rel") $want"
}
export -f fetchOne remoteSize
export s3Base

# One whitespace-separated (accession hash relPath outDir) record per line, fed
# to xargs four arguments at a time.  None of the four can contain whitespace.
grep -v '^#' "$sampleList" | while IFS=$'\t' read -r acc sample cellType hash; do
    [ -z "$acc" ] && continue
    for rel in $files; do
        printf '%s %s %s %s\n' "$acc" "$hash" "$rel" "$outDir"
    done
done | xargs -P "$jobs" -n 4 bash -c 'fetchOne "$0" "$1" "$2" "$3"'

echo "done; verify with fiberSeqCheck.sh $outDir"

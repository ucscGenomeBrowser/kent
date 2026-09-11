#!/bin/bash
# Build the hg38 "HPRC RDTs" track from the bigPsl published by the Marin lab
# (Dana-Farber Cancer Institute) in the hprc-r2-rdt-testhub GitHub repository.
#
# The upstream file is already a valid hg38 bigPsl. We rebuild it rather than
# copying it so that we can add a name index (the upstream file has none), which
# lets people look up a transcript ID in the position box.
#
# Usage: hprcRdtBuild.sh <outDir>

set -beEu -o pipefail

outDir="${1:?usage: hprcRdtBuild.sh <outDir>}"
srcUrl="https://raw.githubusercontent.com/maxgmarin/hprc-r2-rdt-testhub/main/Data/HPRC.PanTx.ClusteredRDTsRepSeqs.SID99C99.AlnToHG38.bb"
chromSizes=/hive/data/genomes/hg38/chrom.sizes
asFile="$(dirname "$0")/../../../lib/bigPsl.as"

cd "$outDir"

echo "downloading $srcUrl"
curl -sSL -o download.bb "$srcUrl"

# unpack to text so we can count and validate before rebuilding
bigBedToBed download.bb stdout | sort -k1,1 -k2,2n > hprcRdt.bed
inCount=$(wc -l < hprcRdt.bed)
echo "alignments in source file: $inCount"

# Every record must sit inside its chromosome. Anything past the end is a real
# problem in the source data, not something to silently drop, so stop here.
awk -F'\t' -v OFS='\t' '
    NR == FNR { size[$1] = $2; next }
    !($1 in size) { bad++; print "not in hg38:", $1 > "/dev/stderr"; next }
    $3 > size[$1] { bad++; printf "past chrom end: %s %s-%s (%s is %s bp)\n", $4, $2, $3, $1, size[$1] > "/dev/stderr" }
    END { if (bad) { print "ERROR: " bad " records out of bounds" > "/dev/stderr"; exit 1 } }
' "$chromSizes" hprcRdt.bed

bedToBigBed -type=bed12+13 -tab -as="$asFile" -extraIndex=name \
    hprcRdt.bed "$chromSizes" hprcRdt.bb

outCount=$(bigBedInfo hprcRdt.bb | grep '^itemCount' | tr -d ', ' | cut -d: -f2)
echo "alignments in bigBed: $outCount"
if [ "$inCount" != "$outCount" ]; then
    echo "ERROR: $inCount in, $outCount out" >&2
    exit 1
fi

# a few numbers worth having in the makeDoc
echo -n "distinct representative transcripts: "
cut -f4 hprcRdt.bed | sort -u | wc -l
echo -n "source haplotypes represented: "
cut -f4 hprcRdt.bed | sed 's/^G_PBKT\.//; s/_PB\..*//' | sort -u | wc -l

rm -f download.bb

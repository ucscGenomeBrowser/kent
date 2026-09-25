#!/bin/bash
# Convert a position range of one SPARK WGS 2026_08 pVCF chunk to a sites VCF,
# splitting the range into pieces that run in parallel on this machine.
# Usage: sparkWgs45kPvcfRange.sh <in.vcf.gz> <groups.txt> <start> <end> <out.vcf.gz> [pieces] [jobs]
#   start/end: 1-based, [start, end)
# Each piece is converted by sparkWgs45kPvcfToSites.sh. Left-alignment can move
# a record a few bases before its piece start, so the pieces are concatenated
# and then sorted.
set -euo pipefail

inVcf=$1
groups=$2
start=$3
end=$4
outVcf=$5
pieces=${6:-48}
jobs=${7:-48}
scriptDir=$(dirname "$(readlink -f "$0")")
work=$outVcf.work
mkdir -p "$work"

step=$(( (end - start + pieces - 1) / pieces ))
for ((i = 0; i < pieces; i++)); do
    s=$(( start + i * step ))
    e=$(( s + step < end ? s + step : end ))
    [ "$s" -lt "$end" ] && printf '%s\t%d\t%d\n' "$(printf 'p%04d' $i)" "$s" "$e"
done > "$work/pieces.txt"

parallel -j "$jobs" --colsep '\t' \
    bash "$scriptDir/sparkWgs45kPvcfToSites.sh" "$inVcf" "$groups" "$work/{1}.bcf" {2} {3} \
    '2>' "$work/{1}.log" < "$work/pieces.txt"

# Concatenate and sort the pieces
bash "$scriptDir/sparkWgs45kPvcfMerge.sh" "$outVcf" \
    $(cut -f1 "$work/pieces.txt" | sed "s|^|$work/|; s|$|.bcf|")
cat "$work"/p*.log | grep -v '^Lines' || true

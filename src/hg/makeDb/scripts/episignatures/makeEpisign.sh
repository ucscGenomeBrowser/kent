#!/bin/bash
# Build the EpiSign track for hg38 from the bed file the EpiSign group sent.
# Run from /hive/data/genomes/hg38/bed/episignatures/episign.
set -beEu -o pipefail

scripts=$(dirname $(readlink -f $0))
sizes=/hive/data/genomes/hg38/chrom.sizes
inBed=${1:-UCSC_EpiSign6.1.0_hg38.bed}

python3 -I $scripts/episignToBed.py $inBed $sizes > episign.bed

bedToBigBed -tab -type=bed9+7 -as=$scripts/episign.as -extraIndex=name \
    episign.bed $sizes episign.bb

bigBedInfo episign.bb

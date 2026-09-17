#!/bin/bash
# Build the MethaDory episignature track for hg38.
# Run from /hive/data/genomes/hg38/bed/episignatures/methaDory, where the two
# files sent by the MethaDory authors have been placed:
#   20260916_episignatures_loci.tsv.gz
#   20260916_episignatures_loci_metadata.xlsx
set -beEu -o pipefail

scripts=$(dirname $(readlink -f $0))
sizes=/hive/data/genomes/hg38/chrom.sizes

bash $scripts/probeCoords.sh probeCoords.tsv

python3 $scripts/methaDoryToBed.py \
    20260916_episignatures_loci.tsv.gz \
    20260916_episignatures_loci_metadata.xlsx \
    probeCoords.tsv \
    methaDory.bed \
    --raOut methaDoryFilters.ra \
    --studyOut studySummary.tsv \
    --locusOut locusSummary.tsv \
    --chromSizes $sizes

# -extraIndex=name lets the position box find a probe by its cg number; it needs
# the searchTable stanza in episignatures.ra to actually be reachable
bedToBigBed -tab -type=bed9+13 -as=$scripts/methaDory.as -extraIndex=name \
    methaDory.bed $sizes methaDory.bb

bigBedInfo methaDory.bb

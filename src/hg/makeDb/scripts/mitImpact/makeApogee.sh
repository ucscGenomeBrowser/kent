#!/bin/bash
# Build the APOGEE2 / nAPOGEE bigWig track for hg38 (chrM only): one bigWig per
# alternative allele with the raw score as bar height, and one bigBed per allele
# with the class color (setColorWith). Run from
# /hive/data/genomes/hg38/bed/mitimpact after makeMitImpact.sh has downloaded the CSV.
set -beEu -o pipefail

rel=${1:?usage: makeApogee.sh <release, e.g. MitImpact4_V059>}
scripts=$(dirname $(readlink -f $0))
sizes=/hive/data/genomes/hg38/chrom.sizes

mkdir -p apogee
python3 $scripts/apogeeToWig.py $rel/$rel.csv apogee

for a in a c g t; do
    bedGraphToBigWig apogee/$a.bedGraph $sizes apogee/$a.bw
    bedToBigBed -tab -type=bed9 apogee/$a.color.bed $sizes apogee/$a.color.bb
done
echo ${rel##*_} > apogee/version.txt

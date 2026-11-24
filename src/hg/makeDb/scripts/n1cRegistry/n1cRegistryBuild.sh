#!/bin/bash
# Build the three N1C Gene Registry bigBeds for hg38 from a download directory.
# usage: n1cRegistryBuild.sh downloadDir outDir
#   downloadDir has the JSON files written by n1cRegistryDownload.sh
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
dl=$(readlink -f $1)
out=$2
sizes=/hive/data/genomes/hg38/chrom.sizes

mkdir -p $out
cd $out
python3 -I $scriptDir/n1cRegistryToBed.py $dl .
bedToBigBed -tab -type=bed9+22 -as=$scriptDir/n1cVariants.as -extraIndex=name,gene \
    n1cVariants.bed $sizes n1cVariants.bb
bedToBigBed -tab -type=bed9+25 -as=$scriptDir/n1cProjects.as -extraIndex=name,gene \
    n1cProjects.bed $sizes n1cProjects.bb
bedToBigBed -tab -type=bed9+11 -as=$scriptDir/n1cMarketed.as -extraIndex=name,gene \
    n1cMarketed.bed $sizes n1cMarketed.bb

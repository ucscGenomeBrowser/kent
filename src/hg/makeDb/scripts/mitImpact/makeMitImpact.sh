#!/bin/bash
# Build the MitImpact track for hg38 (chrM only).
# Run from /hive/data/genomes/hg38/bed/mitimpact. The release is given as the
# first argument, e.g. MitImpact4_V059; its zip is downloaded and unpacked into
# a directory of the same name.
set -beEu -o pipefail

rel=${1:?usage: makeMitImpact.sh <release, e.g. MitImpact4_V059>}
scripts=$(dirname $(readlink -f $0))
sizes=/hive/data/genomes/hg38/chrom.sizes

if [ ! -s $rel/$rel.csv ]; then
    url=https://mitimpact.mcb2lab.org/static/downloads/$rel.zip
    curl -sSf -o $rel.zip.part $url
    mv $rel.zip.part $rel.zip
    # the zip contains the directory $rel/
    unzip -o $rel.zip
fi

twoBitToFa /hive/data/genomes/hg38/hg38.2bit:chrM chrM.fa

python3 $scripts/mitImpactToBed.py $rel/$rel.csv chrM.fa mitImpact.bed

# -extraIndex: the position box finds variants by HGVS name (m.3243A>G) or rs ID,
# via the searchTable stanzas in mitImpact.ra
bedToBigBed -tab -type=bed9+152 -as=$scripts/mitImpact.as -extraIndex=name,dbSnp \
    mitImpact.bed $sizes mitImpact.bb

# dataVersion on the details page: just the release, e.g. V059
echo ${rel##*_} > version.txt
bigBedInfo mitImpact.bb | head -12

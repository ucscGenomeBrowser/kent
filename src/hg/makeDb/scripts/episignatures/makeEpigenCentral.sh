#!/bin/bash
# Build the EpigenCentral episignature track for hg38 from the lab's track hub.
# Run from /hive/data/genomes/hg38/bed/episignatures/epigenCentral.
set -beEu -o pipefail

scripts=$(dirname $(readlink -f $0))
sizes=/hive/data/genomes/hg38/chrom.sizes
url=https://raw.githubusercontent.com/ccmbioinfo/EpigenCentral-UCSC-Genome-Browser/refs/heads/main/episignatures.bb

# Download into .part and only then move it into place, so a resumed or failed
# fetch can never leave a half-written file that looks finished.
if [ ! -s episignatures.upstream.bb ]; then
    curl -sSL -o episignatures.upstream.bb.part "$url"
    mv episignatures.upstream.bb.part episignatures.upstream.bb
fi
md5sum episignatures.upstream.bb

bigBedToBed episignatures.upstream.bb stdout \
    | python3 $scripts/epigenCentralToBed.py \
        --raOut epigenCentralFilters.ra \
        --htmlOut epigenCentralTable.html \
        --refs $scripts/epigenCentralRefs.tsv \
    > epigenCentral.bed

# -extraIndex=name lets the position box find a probe by its cg number; it needs
# the searchTable stanza in episignatures.ra to actually be reachable
bedToBigBed -tab -type=bed9+8 -as=$scripts/epigenCentral.as -extraIndex=name \
    epigenCentral.bed $sizes epigenCentral.bb

bigBedInfo epigenCentral.bb

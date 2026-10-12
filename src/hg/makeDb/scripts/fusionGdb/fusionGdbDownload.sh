#!/bin/bash
# Download the FusionGDB (v1) tables into the current directory.
# Each file goes to <file>.part first and is only moved into place when its
# size matches the server's Content-Length. The ccsm server ignores range
# requests, so the size comes from a HEAD request.
# usage: fusionGdbDownload.sh [file ...]   (default: all files we use)
set -beEu -o pipefail

base=https://ccsm.uth.edu/FusionGDB/tables
files=${@:-"TCGA_ChiTaRS_combined_fusion_information_on_hg19.txt
TCGA_ChiTaRS_combined_fusion_ORF_analyzed_gencode_h19v19.txt
TCGA_ChiTaRS_combined_fusion_ORF_analyzed_gencode_h19v19_fgID.txt
fgene_disease_associations.txt"}

for f in $files; do
    url=$base/$f
    expSize=$(curl -sSI "$url" | tr -d '\r' | awk 'tolower($1)=="content-length:" {print $2}' | tail -1)
    curl -sS --fail -o $f.part "$url"
    gotSize=$(stat -c %s $f.part)
    if [ -n "$expSize" ] && [ "$expSize" != "$gotSize" ]; then
        echo "size mismatch for $f: server $expSize, got $gotSize" >&2
        exit 1
    fi
    mv $f.part $f
    echo "$f $gotSize bytes"
done

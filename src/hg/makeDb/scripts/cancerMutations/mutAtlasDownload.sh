#!/bin/bash
# Download the Cancer Mutation Atlas (CMA) table, the OncoTree cancer type list
# and the Ensembl gene annotation used for the consequence annotation.
# Run in the data directory, e.g. /hive/data/genomes/hg38/bed/cancerVariants/mutAtlas
# usage: mutAtlasDownload.sh <cmaVersion>    e.g. 2026.09.15
set -beEu -o pipefail

ver=${1:?usage: mutAtlasDownload.sh <cmaVersion, e.g. 2026.09.15>}
oncotreeVer=oncotree_2025_10_03
ensVer=115

# download into .part, then mv, so a re-run never appends to a replaced file
get() {
    url=$1; out=$2
    if [ -s "$out" ]; then echo "$out exists, skipping"; return; fi
    rm -f "$out.part"
    curl -sSfL -o "$out.part" "$url"   # oncotree.mskcc.org returns 403 to wget
    mv "$out.part" "$out"
}

get https://cancermutationatlas.org/version/$ver/atlas.tsv atlas.tsv
get "https://oncotree.mskcc.org/api/tumorTypes?version=$oncotreeVer" oncotree.json
get https://ftp.ensembl.org/pub/release-$ensVer/gff3/homo_sapiens/Homo_sapiens.GRCh38.$ensVer.chr.gff3.gz \
    Homo_sapiens.GRCh38.$ensVer.chr.gff3.gz

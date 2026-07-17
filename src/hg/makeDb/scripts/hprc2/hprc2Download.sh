#!/bin/bash
# Download HPRC Release 2 per-assembly "vs GRCh38" Minigraph-Cactus chains from the
# public human-pangenomics S3 bucket (HTTPS, free egress) for the hg38 native tracks.
# Resumable: skips files already present with non-zero size.
# Usage: hprc2Download.sh <chains_grch38.csv> <destDir> [jobs]

set -beEu -o pipefail

csv="${1}"
destDir="${2}"
jobs="${3:-8}"

mkdir -p "${destDir}"

# column 4 of the CSV is the s3:// URL; convert to https and fetch in parallel.
tail -n +2 "${csv}" | cut -d, -f4 \
  | sed 's#^s3://human-pangenomics/#https://human-pangenomics.s3.amazonaws.com/#' \
  | parallel -j "${jobs}" --bar '
      url={};
      fn='"${destDir}"'/$(basename "$url");
      if [ -s "$fn" ]; then exit 0; fi
      curl -sSfL "$url" -o "$fn.part" && mv "$fn.part" "$fn"
    '

echo "downloaded $(ls "${destDir}"/*.chain.gz | wc -l) chain files into ${destDir}"

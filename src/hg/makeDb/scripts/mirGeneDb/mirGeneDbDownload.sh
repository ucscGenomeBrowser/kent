#!/bin/bash
# Download the MirGeneDB human (hsa) annotation into the current directory:
#   hsa-all.bed   all features (pri, pre, arms, seeds, loops, motifs), hg38
#   hsa.gff       precursors and arms with their miRBase MI/MIMAT accessions
#   hsa.browse.html  the browse table: miRBase ID, family, seed per gene
# Each file goes to <file>.part first and is only moved into place when complete.
# hsa-all.bed is a static file, checked against the server's size from a one-byte
# range request. The GFF and the browse page are generated on request and have no
# size, so they are checked for their header instead.
# usage: mirGeneDbDownload.sh
set -beEu -o pipefail

base=https://mirgenedb.org

fetch() {   # fetch <url> <file> [<string the file must contain>]
    url=$1; f=$2; mustHave=${3:-}
    curl -sS --fail -o $f.part "$url"
    gotSize=$(stat -c %s $f.part)
    if [ -z "$mustHave" ]; then
        expSize=$(curl -sS -r 0-0 -D - -o /dev/null "$url" | tr -d '\r' \
            | awk 'tolower($1)=="content-range:" {split($3, a, "/"); print a[2]}')
        if [ "$expSize" != "$gotSize" ]; then
            echo "size mismatch for $f: server $expSize, got $gotSize" >&2
            exit 1
        fi
    elif ! grep -q "$mustHave" $f.part; then
        echo "$f does not contain '$mustHave', incomplete download?" >&2
        exit 1
    fi
    mv $f.part $f
    echo "$f $gotSize bytes"
}

fetch $base/static/data/hsa/hsa-all.bed hsa-all.bed
fetch "$base/gff/hsa?all=1&sort=pos" hsa.gff "# microRNAs:"
fetch $base/browse/hsa hsa.browse.html "</table>"

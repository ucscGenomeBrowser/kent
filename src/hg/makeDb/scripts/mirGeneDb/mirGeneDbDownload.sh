#!/bin/bash
# Download the MirGeneDB annotation of one species (default hsa, human) into the
# current directory, e.g. for mouse (mmu):
#   mmu-all.bed   all features (pri, pre, arms, seeds, loops, motifs)
#   mmu.gff       precursors and arms with their miRBase MI/MIMAT accessions
#   mmu.browse.html  the browse table: miRBase ID, family, seed per gene
# Each file goes to <file>.part first and is only moved into place when complete.
# <species>-all.bed is a static file, checked against the server's size from a one-byte
# range request. The GFF and the browse page are generated on request and have no
# size, so they are checked for their header instead.
# usage: mirGeneDbDownload.sh [species]
set -beEu -o pipefail

sp=${1:-hsa}
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

fetch $base/static/data/$sp/$sp-all.bed $sp-all.bed
fetch "$base/gff/$sp?all=1&sort=pos" $sp.gff "# microRNAs:"
fetch $base/browse/$sp $sp.browse.html "</table>"

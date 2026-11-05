#!/bin/bash
# Download the miRBase GFF3 of one species (default hsa, human) into the current
# directory, e.g. mmu.gff3 for mouse.
# The file goes to <file>.part first and is only moved into place when its size
# matches the server's, from a one-byte range request.
# mirbase.org is often unreachable; rerun later if it fails.
# usage: mirBaseDownload.sh [species]
set -beEu -o pipefail

sp=${1:-hsa}
url=https://mirbase.org/download/$sp.gff3
f=$sp.gff3

curl -sS --fail -o $f.part "$url"
gotSize=$(stat -c %s $f.part)
expSize=$(curl -sS -r 0-0 -D - -o /dev/null "$url" | tr -d '\r' \
    | awk 'tolower($1)=="content-range:" {split($3, a, "/"); print a[2]}')
if [ "$expSize" != "$gotSize" ]; then
    echo "size mismatch for $f: server $expSize, got $gotSize" >&2
    exit 1
fi
mv $f.part $f
echo "$f $gotSize bytes"

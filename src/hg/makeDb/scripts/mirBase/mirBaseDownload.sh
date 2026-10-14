#!/bin/bash
# Download the miRBase human (hsa) GFF3 into the current directory.
# The file goes to <file>.part first and is only moved into place when its size
# matches the server's, from a one-byte range request.
# mirbase.org is often unreachable; rerun later if it fails.
# usage: mirBaseDownload.sh
set -beEu -o pipefail

url=https://mirbase.org/download/hsa.gff3
f=hsa.gff3

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

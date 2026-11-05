#!/bin/bash
# Download every file in manifest.tsv into files/<format>/, 8 at a time,
# skipping files that are already there with the right md5.  Re-runnable.
# Run in the build directory, after encodeE2gManifest.py.
set -beEu -o pipefail
mkdir -p files/bigInteract files/bed files/bedpe

tail -n +2 manifest.tsv | cut -f5,7,8 | \
  xargs -P 8 -L 1 bash -c '
    md5=$0; url=$1; out=$2
    if [ -s "$out" ] && [ "$(md5sum < "$out" | cut -d" " -f1)" = "$md5" ]; then exit 0; fi
    curl -sfL --retry 5 --retry-delay 10 -o "$out.tmp" "$url" || { echo "FAIL curl $url"; exit 0; }
    if [ "$(md5sum < "$out.tmp" | cut -d" " -f1)" = "$md5" ]; then mv -f "$out.tmp" "$out"
    else echo "FAIL md5 $url"; rm -f "$out.tmp"; fi'

tail -n +2 manifest.tsv | awk -F'\t' '{print $5"  "$8}' | md5sum -c --quiet && echo ALL OK

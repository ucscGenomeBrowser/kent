#!/bin/bash
# Verify that a re-encoded GPN-Star bigWig holds exactly the same data as the original:
# identical float32 values and covered intervals on every chromosome (gpnStarCompare.py,
# which fails if it finds no chromosomes), and no bad section headers in the new file.
# usage: gpnStarVerify.sh orig.bw rebuilt.bw
set -beEu -o pipefail
orig=$1; new=$2
dir=$(dirname $0)
$dir/gpnStarCompare.py $orig $new
if ! out=$($dir/gpnStarCheckSections.py $new 0); then
    echo "$out"; echo "FAIL $new section check failed"; exit 1
fi
bad=$(echo "$out" | awk -F': ' '/^sections=/{print $2}')
if [ "$bad" != "0" ]; then
    echo "FAIL $new bad sections: ${bad:-none reported}"; exit 1
fi
echo "OK $new bad sections 0"

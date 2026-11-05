#!/bin/bash
# Re-encode one downloaded GPN-Star bigWig with kent tools to fix its section headers.
# The new file is moved to its final path only after gpnStarVerify.sh passes; on failure
# it is deleted and the script exits non-zero.
# usage: gpnStarRebuild.sh download/in.bw out.bw chrom.sizes tmpDir
set -beEu -o pipefail
in=$1; out=$2; sizes=$3; tmp=$4
wig=$tmp/gpnStarRebuild.$$.$(basename $in).wig
new=$out.rebuilding
trap 'rm -f $wig $new' EXIT
bigWigToWig $in $wig
wigToBigWig $wig $sizes $new
rm -f $wig
if $(dirname $0)/gpnStarVerify.sh $in $new; then
    mv $new $out
else
    echo "FAIL $in, rebuilt file removed"; exit 1
fi

#!/bin/bash
# Re-encode one GPN-Star bigWig with kent tools to fix its section headers, then verify
# with gpnStarVerify.sh that every per-base value is unchanged and no bad headers remain.
# usage: gpnStarRebuild.sh in.bw chrom.sizes tmpDir
# writes <dir of in.bw>/rebuilt/<name>.bw
set -beEu -o pipefail
in=$1; sizes=$2; tmp=$3
dir=$(dirname $in); name=$(basename $in)
mkdir -p $dir/rebuilt
wig=$tmp/gpnStarRebuild.$$.$name.wig
bigWigToWig $in $wig
wigToBigWig $wig $sizes $dir/rebuilt/$name
rm -f $wig
# compare per chromosome: kent writes chromosomes in a different order than the original
$(dirname $0)/gpnStarVerify.sh $in $dir/rebuilt/$name

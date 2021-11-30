#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/maskDelta.sh

usage() {
    echo "usage: $0 treeIn.pb treeOut.pb"
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

treeInPb=$1
treeOutPb=$2

ottoDir=/hive/data/outside/otto/sarscov2phylo

usherDir=~angie/github/usher
matUtils=$usherDir/build/matUtils
matOptimize=$usherDir/build/matOptimize

# I wish there were a less hacky method to identify the node for Delta, but since mutation
# paths can change as new samples are added, this is the most stable method I have at the moment:
# make sample-paths, grep for basal sample IND/GBRC714b/2021 (USA/WI-CDC-FG-038252/2021 would also
# work in case that one goes away for any reason), use the final node in path.
samplePaths=$treeInPb.sample-paths
$matUtils extract -i $treeInPb -S $samplePaths
deltaNode=$(grep IND/GBRC714b/2021 $samplePaths | awk '{print $NF;}' | sed -re 's/:.*//;')

# Delta has deletions at S:157-158 (22029-22034), ORF8:119-120 (28248-28253) and 28271.
# Mask those locations and some adjacent bases where we get a ton of spurious "mutations".
maskFile=$(basename $treeInPb .pb).maskForDelta.tsv
set +x
for ((i=22027;  $i <= 22034;  i++)); do
    echo -e "N${i}N\t$deltaNode"
done > $maskFile
for ((i=28246;  $i <= 28253;  i++)); do
    echo -e "N${i}N\t$deltaNode"
done >> $maskFile
echo -e "N28271N\t$deltaNode" >> $maskFile
set -x

time $matUtils mask -i $treeInPb \
    -m $maskFile \
    -o $treeOutPb

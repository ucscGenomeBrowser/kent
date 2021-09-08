#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateLineageTreePb.sh

usage() {
    echo "usage: $0 buildDate"
}

if [ $# != 1 ]; then
  usage
  exit 1
fi

buildDate=$1

ottoDir=/hive/data/outside/otto/sarscov2phylo

usherDir=~angie/github/usher
matUtils=$usherDir/build/matUtils

today=$(date +%F)
cd $ottoDir/$buildDate

# Get full sample name for lineage A sequence used as reference/root by Pangolin:
wh4SampleName=$(grep LR757995 samples.$buildDate)

# Reroot protobuf:
$matUtils extract -i gisaidAndPublic.$buildDate.masked.pb \
    --reroot "$wh4SampleName" \
    -o gisaidAndPublic.$buildDate.masked.reroot.pb

# Assign updated lineages on the rerooted tree, pango-only for pangolin:
time $matUtils annotate -T 50 \
    -i gisaidAndPublic.$buildDate.masked.reroot.pb \
    -l \
    -c lineageToName \
    -f 0.95 \
    -u mutations.pangoOnly \
    -D details.pangoOnly \
    -o gisaidAndPublic.$buildDate.masked.reroot.pangoOnly.pb \
    >& annotate.pangoOnly.out

set +o pipefail
grep 'Could not' annotate.pangoOnly.out | cat
set -o pipefail

# Make a bunch of smaller trees and see how they do.
mkdir /hive/users/angie/lineageTreeUpdate.$today
cd /hive/users/angie/lineageTreeUpdate.$today
for i in 0 1 2 3 4 5 6 7 8 9; do
    echo test.50.$i
    $matUtils extract -i $ottoDir/$buildDate/gisaidAndPublic.$buildDate.masked.reroot.pangoOnly.pb \
        -r 50 -o test.50.$i.pb
    $matUtils mask -i test.50.$i.pb -S -o test.50.$i.simp.pb
done

# Run pangolin on each subtree
# 7.5-14.5m each job, ~3.5hrs total:
conda activate pangolin
for i in 0 1 2 3 4 5 6 7 8 9; do
    echo test.50.$i
    time pangolin -t 50 --usher-tree test.50.$i.pb \
        --skip-designation-hash --no-temp --outdir subset_10000_0.pusher.test.50.$i.out \
        ../pangolin_eval/subset_10000_0.fa
    echo test.50.$i.simp
    time pangolin -t 50 --usher-tree test.50.$i.simp.pb \
        --skip-designation-hash --no-temp --outdir subset_10000_0.pusher.test.50.$i.simp.out \
        ../pangolin_eval/subset_10000_0.fa
done

# Summarize results
for i in 0 1 2 3 4 5 6 7 8 9; do
    echo test.50.$i
    tail -n+2 subset_10000_0.pusher.test.50.$i.out/lineage_report.csv \
    | awk -F, '{print $1 "\t" $2;}' \
    | sort \
        > subset_10000_0.pusher.test.50.$i
    join -t$'\t' ../pangolin_eval/subset_10000_0.cogNameToLin subset_10000_0.pusher.test.50.$i \
    | tawk '$2 != $3' \
        > subset_10000_0.pusher.test.50.$i.diff
    cut -f 2,3 subset_10000_0.pusher.test.50.$i.diff \
    | sort | uniq -c | sort -nr > subset_10000_0.pusher.test.50.$i.diffrank
    tail -n+2 subset_10000_0.pusher.test.50.$i.simp.out/lineage_report.csv \
    | awk -F, '{print $1 "\t" $2;}' \
    | sort \
        > subset_10000_0.pusher.test.50.$i.simp
    join -t$'\t' ../pangolin_eval/subset_10000_0.cogNameToLin subset_10000_0.pusher.test.50.$i.simp \
    | tawk '$2 != $3' \
        > subset_10000_0.pusher.test.50.$i.simp.diff
    cut -f 2,3 subset_10000_0.pusher.test.50.$i.simp.diff \
    | sort | uniq -c | sort -nr > subset_10000_0.pusher.test.50.$i.simp.diffrank
done

wc -l subset_10000_0.*.diff | sort -n

# Pick the subtree with the fewest differences
set +o pipefail
bestTree=$(wc -l subset_10000_0.*.diff | sort -n | head -1 \
           | sed -re 's/.*pusher\.test\.50\.//; s/\.diff//')
set -o pipefail
echo $bestTree

# Which lineages have the most differences?
head subset_10000_0.pusher.test.50.$bestTree.diffrank 

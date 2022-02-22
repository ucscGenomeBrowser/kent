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

# Get node ID for root of lineage A, used as reference/root by Pangolin:
if [ ! -s clade-paths ]; then
    $matUtils extract -i gisaidAndPublic.$buildDate.masked.pb -C clade-paths
fi
lineageARoot=$(grep ^A$'\t' clade-paths | cut -f 2)

# Reroot protobuf to lineage A:
$matUtils extract -i gisaidAndPublic.$buildDate.masked.pb \
    --reroot $lineageARoot \
    -o gisaidAndPublic.$buildDate.masked.reroot.pb

# Reroot pango.clade-mutations.tsv; also remove B.1.1.529 (Rachel's request):
grep -w ^A $scriptDir/pango.clade-mutations.tsv \
| sed -re 's/T28144C( > )?//;  s/C8782T( > )?//;' \
    > pango.clade-mutations.reroot.tsv
grep -vw ^A $scriptDir/pango.clade-mutations.tsv \
| sed -re 's/\t/\tT8782C > C28144T > /;' \
| grep -vF B.1.1.529 \
    >> pango.clade-mutations.reroot.tsv

# Mask additional bases at the beginning and end of the genome that pangolin masks after
# aligning input sequences.
for ((i=56;  $i <= 265;  i++)); do
    echo -e "N${i}N"
done > maskPangoEnds
for ((i=29674;  $i < 29804;  i++)); do
    echo -e "N${i}N"
done >> maskPangoEnds
$matUtils mask -i gisaidAndPublic.$buildDate.masked.reroot.pb \
    -m maskPangoEnds -o gisaidAndPublic.$buildDate.masked.reroot.pangoMasked.pb

# Assign updated lineages on the rerooted & pango-masked tree, pango-only for pangolin:
time $matUtils annotate -T 50 \
    -i gisaidAndPublic.$buildDate.masked.reroot.pangoMasked.pb \
    -M pango.clade-mutations.reroot.tsv \
    -l \
    -c lineageToName \
    -f 0.95 \
    -u mutations.pangoOnly \
    -D details.pangoOnly \
    -o gisaidAndPublic.$buildDate.masked.reroot.pangoOnly.pb \
    >& annotate.pangoOnly.out

set +o pipefail
grep 'Could not' annotate.pangoOnly.out | cat
grep skip annotate.pangoOnly.out | cat
set -o pipefail

# Make a bunch of smaller trees and see how they do.
mkdir -p /hive/users/angie/lineageTreeUpdate.$today
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
    echo test.50.$i.simp
    time pangolin -t 50 --usher-tree test.50.$i.simp.pb \
        --skip-designation-hash --no-temp --outdir subset_10000_0.pusher.test.50.$i.simp.out \
        ../pangolin_eval/subset_10000_0.fa
done

# Summarize results
for i in 0 1 2 3 4 5 6 7 8 9; do
    echo test.50.$i.simp
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

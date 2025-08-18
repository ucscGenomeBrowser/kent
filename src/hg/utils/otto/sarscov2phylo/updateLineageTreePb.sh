#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateLineageTreePb.sh

usage() {
    echo "usage: $0 buildDate [tree.pb.gz]"
}

if (( $# != 1 && $# != 2 )); then
  usage
  exit 1
fi

buildDate=$1
if [ $# == 2 ]; then
    startingTree=$2
else
    startingTree=gisaidAndPublic.$buildDate.masked.pb.gz
fi

ottoDir=/hive/data/outside/otto/sarscov2phylo

usherDir=~angie/github/usher
matUtils=$usherDir/build/matUtils

today=$(date +%F)
cd $ottoDir/$buildDate

# Remove sequences that have two or more reversions relative to their assigned clade/lineage.
$matUtils summary -i $startingTree --node-stats node-stats
#*** Until BA.2.3.22 gets more samples that don't have bogus reversion on 25000 and 26577,
#*** exempt some samples; also, most of JP.1 has rev on 27383,27384:
cat > pruneRevsExemptions <<EOF
Germany/HH-RKI-I-1073418/2022|EPI_ISL_16330648|2022-12-18
Germany/SH-RKI-I-1071936/2022|EPI_ISL_16329344|2022-12-05
Germany/SH-RKI-I-1074649/2022|EPI_ISL_16396486|2022-12-12
Germany/SH-RKI-I-1071904/2022|EPI_ISL_16329314|2022-12-05
Germany/SH-RKI-I-1071924/2022|EPI_ISL_16329332|2022-12-05
Germany/SH-RKI-I-1071920/2022|EPI_ISL_16329328|2022-12-05
Germany/SH-RKI-I-1071899/2022|EPI_ISL_16329310|2022-12-05
Germany/HH-RKI-I-1029775/2022|EPI_ISL_15901009|2022-11-13
Germany/HH-RKI-I-1105132/2023|EPI_ISL_16940514|2023-01-27
OY160699.1
OY148358.1
OY125285.1
OY165180.1
OY176754.1
OY146516.1
OY142598.1
OY124642.1
OY143123.1
OY151190.1
SouthAfrica/NICD-N55605/2023|EPI_ISL_17859330|2023-05-22
SouthAfrica/NICD-N56015/2023|EPI_ISL_18125227|2023-06-15
SouthAfrica/NICD-N56013/2023|EPI_ISL_18125226|2023-06-15
SouthAfrica/NICD-R10184/2023|EPI_ISL_18341856|2023-09-05
SouthAfrica/NICD-R10169/2023|EPI_ISL_18341854|2023-09-05
SouthAfrica/NICD-R10455/2023|EPI_ISL_18341867|2023-09-12
EOF
tawk '$5 > 1 {print $1;}' node-stats \
| grep -vFwf pruneRevsExemptions \
    > pruneRevs
$matUtils extract -i $startingTree \
    -p -s pruneRevs -O -o gisaidAndPublic.$buildDate.masked.pruneRevs.pb.gz

# Get node ID for root of lineage A, used as reference/root by Pangolin:
$matUtils extract -i gisaidAndPublic.$buildDate.masked.pruneRevs.pb.gz -C clade-paths.prunedRevs
lineageARoot=$(grep ^A$'\t' clade-paths.prunedRevs | cut -f 2)

# Reroot protobuf to lineage A and restrict to low mutation density (highly supported nodes):
$matUtils extract -i gisaidAndPublic.$buildDate.masked.pruneRevs.pb.gz \
    --reroot $lineageARoot \
    --max-mutation-density 2 \
    -O -o gisaidAndPublic.$buildDate.masked.reroot.pb.gz

# Reroot pango.clade-mutations.tsv
grep -w ^A $scriptDir/pango.clade-mutations.tsv \
| sed -re 's/T28144C( > )?//;  s/C8782T( > )?//;' \
    > pango.clade-mutations.reroot.tsv
grep -vw ^A $scriptDir/pango.clade-mutations.tsv \
| sed -re 's/\t/\tT8782C > C28144T > /;' \
| sed -re 's/T8782C >([ >ACGTN0-9,]+)C8782T/\1/; s/> +>/>/;' \
    >> pango.clade-mutations.reroot.tsv

# Mask additional bases at the beginning and end of the genome that pangolin masks after
# aligning input sequences.
set +x
for ((i=56;  $i <= 265;  i++)); do
    echo -e "N${i}N"
done > maskPangoEnds
for ((i=29674;  $i < 29804;  i++)); do
    echo -e "N${i}N"
done >> maskPangoEnds
set -x
$matUtils mask -i gisaidAndPublic.$buildDate.masked.reroot.pb.gz -c \
    -m maskPangoEnds -o gisaidAndPublic.$buildDate.masked.reroot.pangoMasked.pb.gz

# Preserve lineage annotations that survived rerooting and pango-masking
$matUtils extract -i gisaidAndPublic.$buildDate.masked.reroot.pangoMasked.pb.gz \
    -C clade-paths.reroot.pangoMasked
tail -n+2 clade-paths.reroot.pangoMasked \
| grep '^[A-Za-z]' \
| cut -f 1,3 > lineageToPath.reroot.pangoMasked

# Assign updated lineages on the rerooted & pango-masked tree, pango-only for pangolin:
time $matUtils annotate -T 50 \
    -i gisaidAndPublic.$buildDate.masked.reroot.pangoMasked.pb.gz \
    -P lineageToPath.reroot.pangoMasked \
    -M pango.clade-mutations.reroot.tsv \
    -l \
    -c lineageToName \
    -f 0.95 \
    -u mutations.pangoOnly \
    -D details.pangoOnly \
    -o gisaidAndPublic.$buildDate.masked.reroot.pangoOnly.pb.gz \
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
    $matUtils extract -i $ottoDir/$buildDate/gisaidAndPublic.$buildDate.masked.reroot.pangoOnly.pb.gz \
        -r 50 -o test.50.$i.pb
    $matUtils mask -i test.50.$i.pb -S -o test.50.$i.simp.pb
done

# Run pangolin on each subtree
# 7.5-14.5m each job, ~3.5hrs total:
conda activate pangolin
for i in 0 1 2 3 4 5 6 7 8 9; do
    echo test.50.$i.simp
    time pangolin -t 80 --usher-tree test.50.$i.simp.pb --skip-scorpio \
        --skip-designation-cache --no-temp --outdir subset_10000_0.pusher.test.50.$i.simp.out \
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

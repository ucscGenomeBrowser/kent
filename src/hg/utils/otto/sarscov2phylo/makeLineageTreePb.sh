#!/bin/bash
source ~/.bashrc
conda activate bte
set -beEu -o pipefail

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
scriptDir=$(dirname "${BASH_SOURCE[0]}")

usherDir=~angie/github/usher
matUtils=$usherDir/build/matUtils

today=$(date +%F)

cd $ottoDir/$buildDate

# Make a list of leaves to keep in the lineageTree.
python $scriptDir/make_lineage_tree_sample_list.py -i $startingTree -o lineageReps

# Extract the subtree of those leaves.
$matUtils extract -i $startingTree -s lineageReps -o linReps.pb.gz

# Get the node ID of lineage A for rerooting
$matUtils extract -i linReps.pb.gz -C clade-paths.linReps
lineageARoot=$(grep ^A$'\t' clade-paths.linReps | cut -f 2)

# Reroot protobuf to lineage A
$matUtils extract -i linReps.pb.gz --reroot $lineageARoot -o linReps.reroot.pb.gz

# Reroot pango.clade-mutations.tsv
sed -re 's/\t($|[A-Z][0-9]+[A-Z])/\tT8782C > C28144T > \1/;' $scriptDir/pango.clade-mutations.tsv \
    > pango.clade-mutations.reroot.tsv

# Mask additional bases at the beginning and end of the genome that pangolin masks after
# aligning input sequences.
for ((i=56;  $i <= 265;  i++)); do
    echo -e "N${i}N"
done > maskPangoEnds
for ((i=29674;  $i < 29804;  i++)); do
    echo -e "N${i}N"
done >> maskPangoEnds
$matUtils mask -i linReps.reroot.pb.gz -m maskPangoEnds -o linReps.reroot.pangoMasked.pb.gz

# Preserve lineage annotations that survived rerooting and pango-masking
$matUtils extract -i linReps.reroot.pangoMasked.pb.gz -C clade-paths.reroot.pangoMasked
tail -n+2 clade-paths.reroot.pangoMasked \
| grep '^[A-Za-z]' \
| cut -f 1,3 > lineageToPath.reroot.pangoMasked
grep -Fwf lineageReps lineageToName > lineageToName.linReps

# Assign updated lineages on the rerooted & pango-masked tree, pango-only for pangolin:
$matUtils annotate -T 32 \
          -i linReps.reroot.pangoMasked.pb.gz \
          -l \
          -P lineageToPath.reroot.pangoMasked \
          -M pango.clade-mutations.reroot.tsv \
          -c lineageToName.linReps \
          -f 0.95 \
          -o linReps.reroot.pangoOnly.pb.gz \
          >& annotate.pangoOnly.out

# Check for any lost lineage annotations (aside from extra label B.1.1.194_29706T that goes away with pango-masking)
set +o pipefail
couldNot=$(grep 'Could not' annotate.pangoOnly.out | grep -v B.1.1.194_29706T | cat)
skipped=$(grep skip annotate.pangoOnly.out | cat)
if [[ $couldNot != "" || $skipped != "" ]]; then
    echo "Lost lineage(s) in reroot/pango-masking:"
    echo $couldNot
    echo $skipped
    exit 1
fi
set -o pipefail

# Simplify the tree to remove names and private mutations.
$matUtils mask -i linReps.reroot.pangoOnly.pb.gz --simplify -o linReps.reroot.pangoOnly.simp.pb.gz
# Remove BA.5.1_no29666 because it's always a problematic mini-BA.5.1;
$matUtils extract -i linReps.reroot.pangoOnly.simp.pb.gz -p -c BA.5.1_no29666 \
          -o linReps.reroot.pangoOnly.simp.pruned.pb.gz
# Reassign all extra labels (for potential recombinants and sublineages) to valid Pango labels so we don't get a
# regex failure when running pangolin.
$matUtils extract -i linReps.reroot.pangoOnly.simp.pruned.pb.gz -C clade-paths.tmp
tail -n+2 clade-paths.tmp \
| cut -f 1,2 \
| sed -re 's/miscBA[A-Za-z0-9]+/BA.2/;
           s/miscDeltaBA1[A-Za-z0-9]+/BA.1/;
           s/miscBA.5.2CJ.1/CJ.1/;
           s/miscBA.5BA.2.75/BA.5/;
           s/proposed422/BA.1/;
           s/proposed437/BA.1/;
           s/proposed439/BA.1.1/;
           s/proposed441/BA.1/;
           s/proposed446/BA.1.1/;
           s/proposed455/BA.1/;
           s/proposed467/BA.2/;
           s/proposed482/BA.2/;
           s/proposed882/BA.2/;
           s/proposed885/BA.2/;
           s/proposed911/BA.5/;
           s/proposed1006/BA.2/;
           s/proposed1137/BA.5/;
           s/([A-Z\.0-9]+)_[A-Za-z0-9_]+/\1/;' \
    > cladeNodes.reassign
# Make $buildDate's candidate lineage tree
$matUtils annotate -T 32 -i linReps.reroot.pangoOnly.simp.pruned.pb.gz \
          -l -C cladeNodes.reassign -o lineageTree.$buildDate.pb

# Check annotated lineage count: subtract 1 because we pruned BA.5.1_no29666
$matUtils extract -i linReps.reroot.pangoOnly.pb.gz -C clade-paths.reroot.pangoOnly
preSimplifyCount=$(wc -l < clade-paths.reroot.pangoOnly)
$matUtils extract -i lineageTree.$buildDate.pb -C clade-paths.lineageTree
finalCount=$(wc -l < clade-paths.lineageTree)
if (( $finalCount != $preSimplifyCount - 1 )); then
    echo "Expected final lineage count to be $preSimplifyCount - 1, but got $finalCount."
    exit 1
fi


# Clean up
rm linReps.reroot.pb.gz maskPangoEnds linReps.reroot.pangoMasked.pb.gz clade-paths.reroot.pangoMasked
rm lineageToPath.reroot.pangoMasked lineageToName.linReps linReps.reroot.pangoOnly.pb.gz
rm linReps.reroot.pangoOnly.simp.pb.gz linReps.reroot.pangoOnly.simp.pruned.pb.gz clade-paths.tmp
rm clade-paths.reroot.pangoOnly clade-paths.lineageTree cladeNodes.reassign
rm clade-paths.linReps pango.clade-mutations.reroot.tsv
# rm -f debug.tsv
pigz -f -p 8 lineageReps annotate.pangoOnly.out

# Link to public trees download directory hierarchy
archiveRoot=/hive/users/angie/publicTrees
read y m d < <(echo $today | sed -re 's/-/ /g')
archive=$archiveRoot/$y/$m/$d
mkdir -p $archive
ln -f $(pwd)/lineageTree.$buildDate.pb $archive/
ln -f $archive/lineageTree.$buildDate.pb $archiveRoot/lineageTree-latest.pb

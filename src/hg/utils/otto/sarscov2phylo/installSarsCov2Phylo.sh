#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateSarsCov2Phylo.sh

usage() {
    echo "usage: $0 releaseLabel"
}

if [ $# != 1 ]; then
  usage
  exit 1
fi

releaseLabel=$1

ottoDir=/hive/data/outside/otto/sarscov2phylo
gbdbRoot=/gbdb/wuhCor1

cd $ottoDir/$releaseLabel

# GISAID track
for f in gisaid-${releaseLabel}{,.minAf.01,.minAf.001}.vcf.gz; do
    if [ ! -f $f ]; then
        echo "File '$f' not found"
        #*** exit 1
    fi
    if [ ! -f $f.tbi ]; then
        echo "File '$f.tbi' not found"
        #*** exit 1
    fi
    t=$(echo $f | sed -re "s/-$releaseLabel//")
    ln -sf $ottoDir/$releaseLabel/$f $gbdbRoot/sarsCov2Phylo/$t
    ln -sf $ottoDir/$releaseLabel/$f.tbi $gbdbRoot/sarsCov2Phylo/$t.tbi
done
ln -sf $ottoDir/$releaseLabel/gisaid-$releaseLabel.parsimony.bw \
    $gbdbRoot/sarsCov2Phylo/gisaid.parsimony.bw

ln -sf $ottoDir/$releaseLabel/global.reroot.collapsed.nwk $gbdbRoot/sarsCov2Phylo/

for f in {gisaid,lineage,nextstrain}Colors.gz; do
    ln -sf $ottoDir/$releaseLabel/$f $gbdbRoot/sarsCov2Phylo/sarscov2phylo.$f
done

echo "$releaseLabel" > sarscov2phylo-$releaseLabel.version.txt
ln -sf $ottoDir/$releaseLabel/sarscov2phylo-$releaseLabel.version.txt \
    $gbdbRoot/sarsCov2Phylo/sarscov2phylo.version.txt

# Public track
for f in public-$releaseLabel{,.all}{,.minAf.01,.minAf.001}.vcf.gz; do
    if [ ! -f $f ]; then
        echo "File '$f' not found"
        #*** exit 1
    fi
    if [ ! -f $f.tbi ]; then
        echo "File '$f.tbi' not found"
        #*** exit 1
    fi
    t=$(echo $f | sed -re "s/-$releaseLabel//")
    ln -sf $ottoDir/$releaseLabel/$f $gbdbRoot/sarsCov2PhyloPub/$t
    ln -sf $ottoDir/$releaseLabel/$f.tbi $gbdbRoot/sarsCov2PhyloPub/$t.tbi
done
for s in {,.all}.parsimony.bw; do
    ln -sf $ottoDir/$releaseLabel/public-$releaseLabel$s $gbdbRoot/sarsCov2PhyloPub/public$s
done

ln -sf $ottoDir/$releaseLabel/global.reroot.collapsed.public-$releaseLabel.nwk \
    $gbdbRoot/sarsCov2PhyloPub/sarscov2phylo.pub.ft.nh

ln -sf $ottoDir/$releaseLabel/public-$releaseLabel.all.nwk \
    $gbdbRoot/sarsCov2PhyloPub/sarscov2phylo.pub.all.ft.nh

for f in public{Gisaid,Lineage,Nextstrain}Colors.gz; do
    t=$(echo $f | sed -re 's/^public([A-Z])/\l\1/')
    ln -sf $ottoDir/$releaseLabel/$f $gbdbRoot/sarsCov2PhyloPub/sarscov2phylo.pub.$t
done

ln -sf $ottoDir/$releaseLabel/sarscov2phylo-$releaseLabel.version.txt \
    $gbdbRoot/sarsCov2PhyloPub/sarscov2phylo.pub.version.txt

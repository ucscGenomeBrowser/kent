#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updatePublic.sh

usage() {
    echo "usage: $0 problematicSitesVcf"
}

if [ $# != 1 ]; then
  usage
  exit 1
fi

problematicSitesVcf=$1

ottoDir=/hive/data/outside/otto/sarscov2phylo
gisaidDir=/hive/users/angie/gisaid

today=$(date +%F)
scriptDir=$(dirname "${BASH_SOURCE[0]}")

$scriptDir/gisaidFromChunks.sh

cogUkDir=$ottoDir/cogUk.$today
mkdir -p $cogUkDir
cd $cogUkDir
time $scriptDir/getCogUk.sh >& getCogUk.log

ncbiDir=$ottoDir/ncbi.$today
mkdir -p $ncbiDir
cd $ncbiDir
time $scriptDir/getNcbi.sh >& getNcbi.log

time $scriptDir/updateIdMapping.sh \
    $gisaidDir/{metadata_batch_$today.tsv.gz,sequences_batch_$today.fa.xz}

buildDir=$ottoDir/$today
mkdir -p $buildDir
cd $buildDir

prevDate=$(date -d yesterday +%F)
time $scriptDir/updateCombinedTree.sh $prevDate $today $problematicSitesVcf \
    >& updateCombinedTree.log

echo ""
cat hgPhyloPlace.description.txt
cat hgPhyloPlace.plusGisaid.description.txt

grep skip annotate.pango annotate.nextclade
grep 'Could not' annotate.pango annotate.nextclade

# Clean up
nice xz -f new*fa &

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

$scriptDir/gisaidFromChunks.sh &

cogUkDir=$ottoDir/cogUk.$today
mkdir -p $cogUkDir
cd $cogUkDir && time $scriptDir/getCogUk.sh >& getCogUk.log &

ncbiDir=$ottoDir/ncbi.$today
mkdir -p $ncbiDir
cd $ncbiDir && time $scriptDir/getNcbi.sh >& getNcbi.log &

wait

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

set +o pipefail
grep skip annotate.pango annotate.nextclade | cat
grep 'Could not' annotate.pango annotate.nextclade | cat

# Check for newly lineages that are missing from pango.clade-mutations.tsv
set +x
lineages=~angie/github/pango-designation/lineages.csv
tail -n+2 $lineages | cut -d, -f 2 | uniq | grep -E '^(AY|[B-Z][A-Z])' | sort -u \
    > $TMPDIR/designatedDoubleLetters
cut -f 1 $scriptDir/pango.clade-mutations.tsv  \
| grep -E '^(AY|[B-Z][A-Z])' | grep -v _ | sort -u \
    > $TMPDIR/cladeMutDoubleLetters
missingLineages=$(comm -23 $TMPDIR/designatedDoubleLetters $TMPDIR/cladeMutDoubleLetters)
if [[ "$missingLineages" != "" ]]; then
    echo "LINEAGES MISSING FROM lineages.csv:"
    echo $missingLineages
fi
extraLineages=$(comm -13 $TMPDIR/designatedDoubleLetters $TMPDIR/cladeMutDoubleLetters)
if [[ "$extraLineages" != "" ]]; then
    echo "EXTRA LINEAGES (withdrawn?) in pango.clade-mutations.tsv:"
    echo $extraLineages
fi
set -o pipefail
set -x

# Clean up
nice xz -f new*fa &

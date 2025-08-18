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
prevDate=$(date -d yesterday +%F)
scriptDir=$(dirname "${BASH_SOURCE[0]}")

$scriptDir/gisaidFromChunks.sh &

cogUkDir=$ottoDir/cogUk.$today
mkdir -p $cogUkDir
cd $cogUkDir && time $scriptDir/getCogUk.sh >& getCogUk.log &

ncbiDir=$ottoDir/ncbi.$today
mkdir -p $ncbiDir
cd $ncbiDir && time $scriptDir/getNcbi.sh >& getNcbi.log &

cncbDir=$ottoDir/cncb.$today
mkdir -p $cncbDir
cd $cncbDir && time $scriptDir/getCncb.sh >& getCncb.log &

wait

if ! time $scriptDir/updateIdMapping.sh \
              $gisaidDir/{metadata_batch_$today.tsv.gz,sequences_batch_$today.fa.xz} ; then
    echo "*** updateIdMapping failed -- proceeding with .latest versions:"
    ls -l $epiToPublic $ottoDir/ncbi.latest $ottoDir/cogUk.latest $ottoDir/cncb.latest
fi

buildDir=$ottoDir/$today
mkdir -p $buildDir
cd $buildDir

time $scriptDir/updateCombinedTree.sh $prevDate $today $problematicSitesVcf \
    >& updateCombinedTree.log

echo ""
cat hgPhyloPlace.description.txt
cat hgPhyloPlace.plusGisaid.description.txt

set +o pipefail
grep skip annotate.pango annotate.nextclade | cat
grep 'Could not' annotate.pango annotate.nextclade | cat

# Check for lineages that should be annotated on the tree but are not, and vice versa.
set +x
lineages=~angie/github/pango-designation/lineages.csv
tail -n+2 $lineages | cut -d, -f 2 | uniq | sort -u \
    > $TMPDIR/designatedLineages
cut -f 1 $buildDir/clade-paths | egrep '^[A-Z]' | grep -v _ | sort \
    > $TMPDIR/annotatedLineages
designatedNotAnnotated=$(comm -23 $TMPDIR/designatedLineages $TMPDIR/annotatedLineages \
                         | grep -vFwf $scriptDir/designatedNotAnnotated | cat)
if [[ "$designatedNotAnnotated" != "" ]]; then
    echo "MISSING LINEAGES:"
    echo "$designatedNotAnnotated"
else
    echo "No unexpectedly missing lineages, good."
fi
annotatedNotDesignated=$(comm -13 $TMPDIR/designatedLineages $TMPDIR/annotatedLineages \
                         | grep -vFwf $scriptDir/annotatedNotDesignated | cat)
if [[ "$annotatedNotDesignated" != "" ]]; then
    echo "EXTRA LINEAGES (withdrawn?) in tree:"
    echo "$annotatedNotDesignated"
else
    echo "No extra lineages, good."
fi
set -o pipefail
set -x

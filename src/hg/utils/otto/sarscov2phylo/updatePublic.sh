#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updatePublic.sh

usage() {
    echo "usage: $0 prevDate problematicSitesVcf"
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

prevDate=$1
problematicSitesVcf=$2

ottoDir=/hive/data/outside/otto/sarscov2phylo
gisaidDir=/hive/users/angie/gisaid

today=$(date +%F)
scriptDir=$(dirname "${BASH_SOURCE[0]}")

$scriptDir/gisaidFromChunks.sh

cogUkDir=$ottoDir/cogUk.$today
mkdir -p $cogUkDir
cd $cogUkDir
$scriptDir/getCogUk.sh >& getCogUk.log

ncbiDir=$ottoDir/ncbi.$today
mkdir -p $ncbiDir
cd $ncbiDir
$scriptDir/getNcbi.sh >& getNcbi.log

$scriptDir/updateIdMapping.sh $gisaidDir/{metadata_batch_$today.tsv.gz,sequences_batch_$today.fa.xz}

buildDir=$ottoDir/$today
mkdir -p $buildDir
cd $buildDir
$scriptDir/updatePublicTree.sh $prevDate $problematicSitesVcf >& updatePublicTree.log

cat hgPhyloPlace.description.txt

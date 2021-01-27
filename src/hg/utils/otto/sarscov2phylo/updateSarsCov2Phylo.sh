#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateSarsCov2Phylo.sh

usage() {
    echo "usage: $0 releaseLabel metadata_date.tsv.gz epiToPublicAndDate.date"
}

if [ $# != 3 ]; then
  usage
  exit 1
fi

releaseLabel=$1
nextmeta=$2
epiToPublic=$3

scriptDir=$(dirname "${BASH_SOURCE[0]}")
ottoDir=/hive/data/outside/otto/sarscov2phylo
problematicSitesVcf=/hive/data/genomes/wuhCor1/bed/problematicSites/20-08-26/problematic_sites_sarsCov2.vcf
genbankFa=$ottoDir/ncbi.latest/genbank.fa.xz
cogUkFa=$ottoDir/cogUk.latest/cog_all.fasta.xz
cncbFa=$ottoDir/cncb.latest/cncb.nonGenBank.fasta

mkdir -p $ottoDir/$releaseLabel
cd $ottoDir/$releaseLabel

$scriptDir/processGisaid.sh $releaseLabel $treeDir $msaFile $nextmeta $problematicSitesVcf \
    >& processGisaid.log

$scriptDir/mapPublic.sh $releaseLabel $nextmeta $problematicSitesVcf $epiToPublic >& mapPublic.log

$scriptDir/extractUnmappedPublic.sh $epiToPublic $genbankFa $cogUkFa $cncbFa \
    >& extractUnmappedPublic.log

$scriptDir/addUnmappedPublic.sh $releaseLabel >& addUnmappedPublic.log

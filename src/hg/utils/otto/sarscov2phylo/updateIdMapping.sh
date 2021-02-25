#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateIdMapping.sh

usage() {
    echo "usage: $0 metadata_date.tsv.gz sequences_date.fasta.gz"
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

nextmeta=$1
nextfasta=$2

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)
ottoDir=/hive/data/outside/otto/sarscov2phylo
mapScriptDir=~/chris_ncov
# Should use a better location than this...
installDir=/hive/users/angie/gisaid

ncbiDir=$ottoDir/ncbi.$today
mkdir -p $ncbiDir
cd $ncbiDir
$scriptDir/getNcbi.sh >& getNcbi.log

cogUkDir=$ottoDir/cogUk.$today
mkdir -p $cogUkDir
cd $cogUkDir
$scriptDir/getCogUk.sh >& getCogUk.log

# Last time I checked, CNCB had not updated since September, just keep using what we have
cncbDir=$ottoDir/cncb.latest

# Set up input files for Chris's scripts to map GISAID <--> public sequences
cd $mapScriptDir
mkdir input/$today
cd input/$today
ln -sf $cncbDir/cncb.nonGenBank.fasta .
ln -sf $ncbiDir/genbank.fa.xz .
ln -sf $cogUkDir/cog_all.fasta.xz .
ln -sf $nextfasta .
xcat $nextmeta | tail -n+2 | cut -f1,3 | uniq > seqToEpi

cd $mapScriptDir
./build.sh -t $today

cd $installDir

gbToDate=$ncbiDir/gbToDate
cogUkToDate=$cogUkDir/cogUkToDate
cncbToDate=$cncbDir/cncbToDate

join -t$'\t' -a 1 -1 2 -o 1.1,1.2,1.3,2.2 \
    <(sort -k2,2 ~/chris_ncov/epiToPublicIdName.$today.txt) \
    <(sort $gbToDate $cncbToDate $cogUkToDate) \
| sort \
    > epiToPublicAndDate.$today

ln -sf epiToPublicAndDate.$today epiToPublicAndDate.latest

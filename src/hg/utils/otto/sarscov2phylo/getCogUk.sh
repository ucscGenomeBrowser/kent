#!/bin/bash

set -beEux -o pipefail

# Download latest COG-UK fasta and metadata; update $ottoDir/cogUk.latest link.

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)

ottoDir=/hive/data/outside/otto/sarscov2phylo
cogUrlBase=https://cog-uk.s3.climb.ac.uk/phylogenetics/latest

mkdir -p $ottoDir/cogUk.$today
cd $ottoDir/cogUk.$today

wget -q $cogUrlBase/cog_all.fasta
wget -q $cogUrlBase/cog_metadata.csv

xz -f -T 50 cog_all.fasta

tail -n +2 cog_metadata.csv \
| awk -F, '{print $1 "\t" $4;}' | sort > cogUkToDate

rm -f $ottoDir/cogUk.latest
ln -s cogUk.$today $ottoDir/cogUk.latest

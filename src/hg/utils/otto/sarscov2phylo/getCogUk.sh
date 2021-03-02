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

curl -S -s $cogUrlBase/cog_all.fasta | xz -T 50 > cog_all.fasta.xz
curl -S -s $cogUrlBase/cog_metadata.csv > cog_metadata.csv
curl -S -s $cogUrlBase/cog_global_tree.newick > cog_global_tree.newick

tail -n +2 cog_metadata.csv \
| awk -F, '{print $1 "\t" $5;}' | sort > cogUkToDate

# Reuse nextclade assignments for older sequences; compute nextclade assignments for new seqs.
cp $ottoDir/cogUk.latest/nextclade.tsv .
comm -13 <(cut -f 1 nextclade.tsv | sort) <(fastaNames cog_all.fasta.xz | sort) \
    > seqsForNextclade
if [ -s seqsForNextclade ]; then
    faSomeRecords <(xzcat cog_all.fasta.xz) seqsForNextclade seqsForNextclade.fa
    splitDir=splitForNextclade
    rm -rf $splitDir
    mkdir $splitDir
    faSplit about seqsForNextclade.fa 30000000 $splitDir/chunk
    for chunkFa in $splitDir/chunk*.fa; do
        nextclade -j 50 -i $chunkFa -t >(cut -f 1,2 | tail -n+2 >> nextclade.tsv) >& nextclade.log
    done
    rm -rf $splitDir
fi

rm -f $ottoDir/cogUk.latest
ln -s cogUk.$today $ottoDir/cogUk.latest

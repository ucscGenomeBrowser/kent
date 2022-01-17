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

# Sometimes the curl fails with a DNS error, regardless of whether my previous cron job with
# curl -I succeeded.  Do multiple retries for the first URL; once it's working, it should
# continue to work for the other URLs (she said hopefully).
attempt=0
maxAttempts=5
retryDelay=60
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "curl attempt $attempt"
    if curl -S -s $cogUrlBase/cog_all.fasta | xz -T 8 > cog_all.fasta.xz; then
        break
    else
        echo "FAILED; will try again after $retryDelay seconds"
        rm -f cog_all.fasta.xz
        sleep $retryDelay
    fi
done
if [[ ! -f cog_all.fasta.xz ]]; then
    echo "curl failed $maxAttempts times; quitting."
    exit 1
fi
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
    nDataDir=~angie/github/nextclade/data/sars-cov-2
    outDir=$(mktemp -d)
    outTsv=$(mktemp)
    for chunkFa in $splitDir/chunk*.fa; do
        nextclade -j 50 -i $chunkFa \
            --input-root-seq $nDataDir/reference.fasta \
            --input-tree $nDataDir/tree.json \
            --input-qc-config $nDataDir/qc.json \
            --output-dir $outDir \
            --output-tsv $outTsv >& nextclade.log
        cut -f 1,2 $outTsv | tail -n+2 | sed -re 's/"//g;' >> nextclade.tsv
        rm $outTsv
    done
    rm -rf $outDir
    rm -rf $splitDir
fi

rm -f $ottoDir/cogUk.latest
ln -s cogUk.$today $ottoDir/cogUk.latest

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
# curl -I succeeded.  Do multiple retries per URL -- sometimes even after one succeeds, the
# next can still fail e.g. with "curl: (6) Could not resolve host: cog-uk.s3.climb.ac.uk".
# On some days, the fetch started but failed partway through, even after 5 tries, so
# keep the partial result around and try again with '-C -'.
function curlRetry {
    local url=$*
    local attempt=0
    local maxAttempts=100
    local retryDelay=60
    while [[ $((++attempt)) -le $maxAttempts ]]; do
        echo "curl attempt $attempt"
        if curl -S -s -C - -O $url; then
            break
        else
            echo "FAILED; will try again after $retryDelay seconds"
            sleep $retryDelay
        fi
    done
    if [[ $attempt -gt $maxAttempts ]]; then
        echo "curl failed $maxAttempts times; quitting."
        exit 1
    fi
}

curlRetry $cogUrlBase/cog_all.fasta.gz
curlRetry $cogUrlBase/cog_metadata.csv.gz
curlRetry $cogUrlBase/cog_global_tree.newick

zcat cog_all.fasta.gz | xz -T 20 > cog_all.fasta.xz
rm cog_all.fasta.gz

zcat cog_metadata.csv.gz \
| tail -n +2 \
| awk -F, '{print $1 "\t" $5;}' | sort > cogUkToDate

# Reuse nextclade assignments for older sequences; compute nextclade assignments for new seqs.
zcat $ottoDir/cogUk.latest/nextclade.full.tsv.gz > nextclade.full.tsv
cp $ottoDir/cogUk.latest/nextalign.fa.xz .
comm -13 <(cut -f 2 nextclade.full.tsv | sort) <(fastaNames cog_all.fasta.xz | sort) \
    > seqsForNextclade
if [ -s seqsForNextclade ]; then
    nDataDir=~angie/github/nextclade/data/sars-cov-2
    outTsv=$(mktemp)
    outFa=$(mktemp)
    faSomeRecords <(xzcat cog_all.fasta.xz) seqsForNextclade stdout \
    | nextclade run -j 30 \
        --input-dataset $nDataDir \
        --output-fasta $outFa \
        --output-tsv $outTsv >& nextclade.log
    tail -n+2 $outTsv | sed -re 's/"//g;' >> nextclade.full.tsv
    xz -T 20 < $outFa >> nextalign.fa.xz
    rm -f $outTsv $outFa
fi
pigz -f -p 8 nextclade.full.tsv

rm -f $ottoDir/cogUk.latest
ln -s cogUk.$today $ottoDir/cogUk.latest

rm -f ~angie/public_html/sarscov2phylo/cogUk.$today
ln -s $ottoDir/cogUk.$today ~angie/public_html/sarscov2phylo/cogUk.$today

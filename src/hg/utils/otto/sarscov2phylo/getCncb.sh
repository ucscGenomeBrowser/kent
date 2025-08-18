#!/bin/bash

set -beEux -o pipefail

# Download latest CNCB/NGDC fasta and metadata; update $ottoDir/cncb.latest link.

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)

ottoDir=/hive/data/outside/otto/sarscov2phylo

metadataUrl='https://ngdc.cncb.ac.cn/ncov/api/es/genome/download?q=&accession=&country=&province=&city=&host=&minCollectDate=&maxCollectDate=&source=CNGBdb,GenBase,Genome%20Warehouse,NMDC&qc=&complete=&minLength=15000&maxLength=31000&ns=&ds=&lineage=&whoLabel=&latest=0&md5=0&md5value=&minSubDate=&maxSubDate='
genBaseFileApiBase='https://ngdc.cncb.ac.cn/genbase/api/file/fasta?acc='

mkdir -p $ottoDir/cncb.$today
cd $ottoDir/cncb.$today

function curlRetry {
    local url=$*
    local attempt=0
    local maxAttempts=20
    local retryDelay=300
    while [[ $((++attempt)) -le $maxAttempts ]]; do
        >&2 echo "curl attempt $attempt"
        if curl -Ss $url; then
            break
        else
            >&2 echo "FAILED; will try again after $retryDelay seconds"
            sleep $retryDelay
        fi
    done
    if [[ $attempt -gt $maxAttempts ]]; then
        >&2 echo "curl failed $maxAttempts times; quitting."
        exit 1
    fi
}

# Discard Pango lineage column so we can keep the same column order as before & won't have to
# update scripts.  Also watch out for non-ASCII characters (e.g. NMDC60013002-06's "2019?"
# was changed to "2019\302\240" -- change it back).
curlRetry $metadataUrl \
| cut -f 1-4,6- \
| perl -wpe 's/[^[:print:]^\t^\n]+/?/g;' \
    > cncb.metadata.tsv

colCount=$(head -1 cncb.metadata.tsv | tawk '{print NF;}')
if [[ $colCount != 16 ]]; then
    echo "Metadata format error: expected 16 columns, got $colCount"
    exit 1
fi

# Make a cncbToDate file for ID mapping.
tail -n+2 cncb.metadata.tsv \
| cut -f 1,10 \
| cleanCncb \
| sort -u \
| sed -re 's/ //;' \
    > cncbToDate

# Make a renaming file to translate from accessions to the 'name | acc' headers expected by
# Chris's ID-matching pipeline.  Exclude sequences that are also in GenBank.  If for some reason
# the EPI_ ID is listed as primary and CNCB as secondary, swap them.
tail -n+2 cncb.metadata.tsv \
| grep -vE '[,'$'\t''][A-Z]{2}[0-9]{6}\.[0-9]+['$'\t'',]' \
| tawk '{ if ($4 == "null") { $4 = ""; } if ($2 ~ /^EPI_ISL/) { tmp = $2; $2 = $4; $4 = tmp; } print $1, $2; }' \
| cleanCncb \
| sort -u \
| sed -re 's/ //;' \
| tawk '{print $2, $1 " | " $2;}' \
| sort \
    > accToNameBarAcc.tsv

# See what sequences are in metadata but that we haven't already downloaded.
fastaNames ../cncb.latest/cncb.nonGenBank.acc.fasta.xz | sort -u > prevAccs
comm -23 <(cut -f 1 accToNameBarAcc.tsv) prevAccs | sort -u > missingIDs

# Use the GenBase API to download missing GenBase sequences, unfortunately only one sequence
# at a time.
tail -n+2 cncb.metadata.tsv \
| grep -vE '[,'$'\t''][A-Z]{2}[0-9]{6}\.[0-9]+['$'\t'',]' \
| tawk '{ if ($4 == "null") { $4 = ""; } if ($2 ~ /^EPI_ISL/) { tmp = $2; $2 = $4; $4 = tmp; } print $2; }' \
| sort -u \
    > nonGenBankIds
set +o pipefail
grep ^C_ missingIDs \
| grep -Fwf - nonGenBankIds \
| cat \
    > missingGenBaseIds
set -o pipefail
for acc in $(cat missingGenBaseIds); do
    curlRetry "$genBaseFileApiBase$acc" \
    | sed -re '/^>/  s/ .*//;'
    sleep 5
done > new.accs.fa

cat <(xzcat ../cncb.latest/cncb.nonGenBank.acc.fasta.xz) new.accs.fa \
| faUniqify stdin stdout \
| xz -T 20 > cncb.nonGenBank.acc.fasta.new.xz
mv cncb.nonGenBank.acc.fasta.new.xz cncb.nonGenBank.acc.fasta.xz

xzcat cncb.nonGenBank.acc.fasta.xz \
| faSomeRecords stdin <(cut -f 1 accToNameBarAcc.tsv) stdout \
| faRenameRecords stdin accToNameBarAcc.tsv stdout \
| xz -T 20 > cncb.nonGenBank.fasta.xz

# Run nextclade
cp ../cncb.latest/nextclade.full.tsv.gz .
cp ../cncb.latest/nextclade.tsv .
if [ -s new.accs.fa ]; then
    nDataDir=~angie/github/nextclade/data/sars-cov-2
    time nextclade run -j 20 new.accs.fa \
        --input-dataset $nDataDir \
        --output-fasta nextalign.new.fa.xz \
        --output-tsv nextclade.new.full.tsv.gz  >& nextclade.log
    zcat nextclade.new.full.tsv.gz | cut -f 2,8 | tail -n+2 >> nextclade.tsv
    sort -u nextclade.tsv > tmp
    mv tmp nextclade.tsv
    cat nextclade.new.full.tsv.gz >> nextclade.full.tsv.gz
fi

# Run pangolin
cp ../cncb.latest/pangolin.tsv .
if [ -s new.accs.fa ]; then
    set +x
    . ~/.bashrc
    conda activate pangolin
    set -x
    set -e
    time pangolin -t 20 new.accs.fa --skip-scorpio --outfile lineages.csv \
        >& pangolin.log
    awk -F, '{print $1 "\t" $2;}' lineages.csv | tail -n+2 >> pangolin.tsv
    sort -u pangolin.tsv > tmp
    mv tmp pangolin.tsv
    conda deactivate
fi

rm new.accs.fa

rm -f $ottoDir/cncb.latest
ln -s cncb.$today $ottoDir/cncb.latest

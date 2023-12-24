#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

# Download SARS-CoV-2 GenBank FASTA and metadata using NCBI Datasets API.
# Use BioSample metadata to fill in gaps in GenBank metadata and report conflicting dates.
# Use enhanced metadata to rewrite FASTA headers for matching up with sequences in other databases.

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)

ottoDir=/hive/data/outside/otto/sarscov2phylo

mkdir -p $ottoDir/ncbi.$today
cd $ottoDir/ncbi.$today

attempt=0
maxAttempts=5
retryDelay=300
#*** From Eric Cox 1/25/22 when download failed and they were debugging firewall issues:
#             --proxy https://www.st-va.ncbi.nlm.nih.gov/datasets/v1 \
#*** From Mirian Tsuchiya 6/3/22: add --debug; if there's a problem send Ncbi-Phid.
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "datasets attempt $attempt"
    if datasets download virus genome taxon 2697049 \
            --include genome,annotation,biosample \
            --filename ncbi_dataset.zip \
            --no-progressbar \
            --debug \
            >& datasets.log.$attempt; then
        break;
    else
        echo "FAILED; will try again after $retryDelay seconds"
        if [[ -f ncbi_dataset.zip ]]; then
            mv ncbi_dataset.zip{,.fail.$attempt}
        fi
        if [[ $attempt -lt $maxAttempts ]]; then
            sleep $retryDelay
        fi
        # Double the delay to give NCBI progressively more time
        retryDelay=$(($retryDelay * 2))
    fi
done
if [[ ! -f ncbi_dataset.zip ]]; then
    echo "datasets command failed $maxAttempts times; quitting."
    exit 1
fi
rm -rf ncbi_dataset
unzip -o ncbi_dataset.zip
# Creates ./ncbi_dataset/

# This makes something just like ncbi.datasets.tsv from the /table/ API query:
time jq -c -r '[.accession, .biosample, .isolate.collectionDate, .location.geographicLocation, .host.sciName, .isolate.name, .completeness, (.length|tostring)] | join("\t")' \
    ncbi_dataset/data/data_report.jsonl \
| sed -e 's/COMPLETE/complete/; s/PARTIAL/partial/;' \
| sort -u \
    > ncbi_dataset.tsv

# Make sure we didn't get a drastically truncated download despite apparently successful commands
minSeqCount=6800000
metadataLC=$(wc -l < ncbi_dataset.tsv)
if (($metadataLC < $minSeqCount)); then
    echo "TOO FEW SEQUENCES: $metadataLC lines of ncbi_dataset.tsv, expected at least $minSeqCount"
    exit 1
fi

time $scriptDir/bioSampleJsonToTab.py ncbi_dataset/data/biosample_report.jsonl \
    | uniq > gb.bioSample.tab

# Use BioSample metadata to fill in missing pieces of GenBank metadata and report conflicting
# sample collection dates:
$scriptDir/gbMetadataAddBioSample.pl gb.bioSample.tab ncbi_dataset.tsv \
    > ncbi_dataset.plusBioSample.tsv 2>gbMetadataAddBioSample.log

# Make a file for joining collection date with ID:
tawk '$3 != "" {print $1, $3;}' ncbi_dataset.plusBioSample.tsv \
| sort > gbToDate

# Replace FASTA headers with reconstructed names from enhanced metadata.
time cleanGenbank < ncbi_dataset/data/genomic.fna \
| $scriptDir/fixNcbiFastaNames.pl ncbi_dataset.plusBioSample.tsv \
    > genbank.maybeDups.fa
time fastaNames genbank.maybeDups.fa | awk '{print $1 "\t" $0;}' > gb.rename
time faUniqify genbank.maybeDups.fa stdout \
| faRenameRecords stdin gb.rename stdout \
| xz -T 20 \
    > genbank.fa.xz

# Run pangolin and nextclade on sequences that are new since yesterday
export TMPDIR=/dev/shm
fastaNames genbank.fa.xz | awk '{print $1;}' | sed -re 's/\|.*//' | grep -vx pdb | sort -u > gb.names
zcat ../ncbi.latest/nextclade.full.tsv.gz > nextclade.full.tsv
cp ../ncbi.latest/nextalign.fa.xz .
comm -23 gb.names <(cut -f 1 nextclade.full.tsv | sort -u) > nextclade.names
if [ -s nextclade.names ]; then
    nDataDir=~angie/github/nextclade/data/sars-cov-2
    outTsv=$(mktemp)
    outFa=$(mktemp)
    faSomeRecords <(xzcat genbank.fa.xz) nextclade.names stdout \
    | sed -re 's/^>([A-Z]{2}[0-9]{6}\.[0-9])[ |].*/>\1/;' \
    | nextclade run -j 50 \
            --input-dataset $nDataDir \
            --output-fasta $outFa \
            --output-tsv $outTsv >& nextclade.log
    tail -n+2 $outTsv | sed -re 's/"//g;' >> nextclade.full.tsv
    xz -T 20 < $outFa >> nextalign.fa.xz
    rm $outTsv $outFa
fi
wc -l nextclade.full.tsv
pigz -f -p 8 nextclade.full.tsv

set +x
conda activate pangolin
set -x
runPangolin() {
    fa=$1
    out=$fa.pangolin.csv
    logfile=$(mktemp)
    pangolin -t 4 $fa --skip-scorpio --outfile $out > $logfile 2>&1
    rm $logfile
}
export -f runPangolin
if [ -e ../ncbi.latest/lineage_report.csv ]; then
    cp ../ncbi.latest/lineage_report.csv linRepYesterday
    tail -n+2 linRepYesterday | sed -re 's/^([A-Z]+[0-9]+\.[0-9]+).*/\1/' | sort \
        > pangolin.prev.names
    comm -23 gb.names pangolin.prev.names > pangolin.names
    faSomeRecords <(xzcat genbank.fa.xz) pangolin.names stdout \
    | sed -re '/^>/  s/^>([A-Z]{2}[0-9]{6}\.[0-9]+) \|[^,]+/>\1/;' \
        > pangolin.fa
    pangolin --skip-scorpio pangolin.fa >& pangolin.log
    tail -n+2 lineage_report.csv >> linRepYesterday
    mv linRepYesterday lineage_report.csv
    rm -f pangolin.fa
else
    splitDir=splitForPangolin
    rm -rf $splitDir
    mkdir $splitDir
    faSplit about <(xzcat genbank.fa.xz | sed -re '/^>/ s/ .*//;') 30000000 $splitDir/chunk
    find $splitDir -name chunk\*.fa \
    | parallel -j 10 "runPangolin {}"
    head -1 $(ls -1 $splitDir/chunk*.csv | head -1) > lineage_report.csv
    for f in $splitDir/chunk*.csv; do
        tail -n+2 $f >> lineage_report.csv
    done
    rm -rf $splitDir
fi
wc -l lineage_report.csv

# It turns out that sometimes new sequences sneak into ncbi_dataset.tsv before they're included in
# genomic.fna.  Filter those out so we don't have missing pangolin and nextclade for just-added
# COG-UK sequences.  (https://github.com/theosanderson/taxodium/issues/10#issuecomment-876800176)
grep -Fwf gb.names ncbi_dataset.plusBioSample.tsv > tmp
wc -l tmp ncbi_dataset.plusBioSample.tsv
mv tmp ncbi_dataset.plusBioSample.tsv

rm -f $ottoDir/ncbi.latest
ln -s ncbi.$today $ottoDir/ncbi.latest

rm -f ~angie/public_html/sarscov2phylo/ncbi.$today
ln -s $ottoDir/ncbi.$today ~angie/public_html/sarscov2phylo/ncbi.$today

# Clean up
rm -r ncbi_dataset genbank.maybeDups.fa

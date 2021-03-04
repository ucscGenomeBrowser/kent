#!/bin/bash

set -beEu -o pipefail

# Download SARS-CoV-2 GenBank FASTA and metadata using NCBI Datasets API.
# Use E-Utils to get SARS-CoV-2 metadata from BioSample.
# Use BioSample metadata to fill in gaps in GenBank metadata and report conflicting dates.
# Use enhanced metadata to rewrite FASTA headers for matching up with sequences in other databases.

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)

ottoDir=/hive/data/outside/otto/sarscov2phylo

mkdir -p $ottoDir/ncbi.$today
cd $ottoDir/ncbi.$today

# This query gives a large zip file that includes extra stuff, and the download really slows
# down after a point, but it does provide a consistent set of FASTA and metadata:
time curl -s -X GET \
    "https://api.ncbi.nlm.nih.gov/datasets/v1alpha/virus/taxon/2697049/genome/download?refseq_only=false&annotated_only=false&released_since=2020-01-01&complete_only=false&include_annotation_type=DEFAULT&filename=ncbi_dataset.$today.zip" \
    -H "Accept: application/zip" \
    > ncbi_dataset.zip

rm -rf ncbi_dataset
unzip ncbi_dataset.zip
# Creates ./ncbi_dataset/

# This makes something just like ncbi.datasets.tsv from the /table/ API query:
jq -c -r '[.accession, .biosample, .isolate.collectionDate, .location.geographicLocation, .host.sciName, .isolate.name, .completeness, (.length|tostring)] | join("\t")' \
    ncbi_dataset/data/data_report.jsonl \
| sed -e 's/COMPLETE/complete/; s/PARTIAL/partial/;' \
| sort \
    > ncbi_dataset.tsv

# Use EUtils (esearch) to get all SARS-CoV-2 BioSample GI# IDs:
$scriptDir/searchAllSarsCov2BioSample.sh

# Use EUtils (efetch) to get BioSample records for all of those GI# IDs:
time $scriptDir/bioSampleIdToText.sh < all.biosample.gids.txt > all.bioSample.txt
gidCount=$(wc -l < all.biosample.gids.txt)
accCount=$(grep ^Accession all.bioSample.txt | sort -u | wc -l)
if [ $gidCount != $accCount ]; then
    echo "Number of Accession lines ($accCount) does not match number of numeric IDs ($gidCount)"
    grep ^Accession all.bioSample.txt | sed -re 's/^.*ID: //' | sort > ids.loaded
    sort all.biosample.gids.txt > all.biosample.gids.sorted.txt
    comm -23 all.biosample.gids.sorted.txt ids.loaded > ids.notLoaded
    echo Retrying queries for `wc -l < ids.notLoaded` IDs
    $scriptDir/bioSampleIdToText.sh < ids.notLoaded >> all.bioSample.txt
    accCount=$(grep ^Accession all.bioSample.txt | sort -u | wc -l)
    if [ $gidCount != $accCount ]; then
        echo "Still have only $accCount accession lines after retrying; quitting."
        exit 1
    fi
fi

# Extract properties of interest into tab-sep text:
$scriptDir/bioSampleTextToTab.pl < all.bioSample.txt  > all.bioSample.tab

# Extract BioSample tab-sep lines just for BioSample accessions included in the ncbi_dataset data:
tawk '$2 != "" {print $2;}' ncbi_dataset.tsv \
| grep -Fwf - all.bioSample.tab \
    > gb.bioSample.tab

# Use BioSample metadata to fill in missing pieces of GenBank metadata and report conflicting
# sample collection dates:
$scriptDir/gbMetadataAddBioSample.pl gb.bioSample.tab ncbi_dataset.tsv \
    > ncbi_dataset.plusBioSample.tsv

# Make a file for joining collection date with ID:
tawk '$3 != "" {print $1, $3;}' ncbi_dataset.plusBioSample.tsv \
| sort > gbToDate

# Replace FASTA headers with reconstructed names from enhanced metadata.
time cleanGenbank < ncbi_dataset/data/genomic.fna \
| $scriptDir/fixNcbiFastaNames.pl ncbi_dataset.plusBioSample.tsv \
| xz -T 50 \
    > genbank.fa.xz

rm -f $ottoDir/ncbi.latest
ln -s ncbi.$today $ottoDir/ncbi.latest

# Clean up
rm -r ncbi_dataset
nice xz all.bioSample.* &

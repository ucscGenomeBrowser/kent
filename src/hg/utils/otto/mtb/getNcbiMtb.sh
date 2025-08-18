#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

# Download M. tuberculosis (taxid ???) from NCBI.
# assembly              Organism name          NC_            taxid    GenBank
# GCF_000195955.2       M. tuberculosis H37Rv  NC_000962.3    83332    AL123456
# taxid 1773 gets many more genomes


today=$(date +%F)

mtbDir=/hive/data/outside/otto/mtb

minSize=4000000

mkdir -p $mtbDir/ncbi/ncbi.$today
cd $mtbDir/ncbi/ncbi.$today

# Download all M. tuberculosis sequences, sort out subtypes later.
taxId=77643

attempt=0
maxAttempts=5
retryDelay=300
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "fasta attempt $attempt"
    if datasets download genome taxon $taxId --include genome; then
        break;
    else
        echo "FAILED fasta; will try again after $retryDelay seconds"
        rm -f ncbi_dataset.zip
        sleep $retryDelay
        # Double the delay to give NCBI progressively more time
        retryDelay=$(($retryDelay * 2))
    fi
done
if [[ ! -s ncbi_dataset.zip ]]; then
    echo "datasets query failed $maxAttempts times; quitting."
    exit 1
fi
unzip ncbi_dataset.zip

# Extract metadata for single-sequence GCA assemblies only from assembly_data_report.jsonl
jq -c -r 'if .assemblyStats.numberOfComponentSequences == 1 then ([.accession, .assemblyInfo.biosample.accession, (.assemblyInfo.biosample.attributes[] | select(.name == "collection_date") | .value) // "", (.assemblyInfo.biosample.attributes[] | select(.name == "geo_loc_name") | .value) // "", (.assemblyInfo.biosample.attributes[] | select(.name == "host") | .value) // "", .assemblyInfo.assemblyName, .assemblyStats.totalSequenceLength, .assemblyInfo.bioprojectAccession // "", (if .assemblyInfo.biosample.sampleIds != null then .assemblyInfo.biosample.sampleIds[] | select(.db == "SRA") | .value else "" end) // "", (.assemblyInfo.biosample.attributes[] | select(.name == "strain") | .value) // "", .organism.organismName, .organism.infraspecificNames.strain // "", .organism.infraspecificNames.isolate // ""] | join("\t")) else "" end' \
    ncbi_dataset/data/assembly_data_report.jsonl \
| grep ^GCA \
| sort \
    > assembly_metadata.tsv

# Unfortunately the correspondence between assembly accession and fasta genbank accession is
# encoded in the fasta directory/filenames, so we have to resolve that one GCA directory at a time.
for d in $(cut -f 1 assembly_metadata.tsv); do
    echo -n -e "$d\t"
    head -1 ncbi_dataset/data/$d/$d*.fna | sed -re 's/^>//; s/ .*//;'
done | sort > assemblyToNuc.tsv
# Build fasta using only the GCA assemblies.
for d in $(cut -f 1 assembly_metadata.tsv); do
    sed -re '/^>/ s/ .*//' ncbi_dataset/data/$d/$d*.fna
done \
| faFilter -minSize=$minSize stdin stdout \
| xz -T 20 > genbank.fa.xz
faSize <(xzcat genbank.fa.xz)

# Add nucleotide accessions used in fasta files to metadata.
join -t$'\t' assemblyToNuc.tsv assembly_metadata.tsv \
| cut -f 2- \
    > metadata.tsv

# Make sure the download wasn't truncated without reporting an error:
count=$(wc -l < metadata.tsv)
minSamples=670
if (( $count < $minSamples )); then
    echo "*** Too few samples ($count)!  Expected at least $minSamples.  Halting. ***"
    exit 1
fi

rm -rf ncbi_dataset

rm -f $mtbDir/ncbi/ncbi.latest
ln -s ncbi.$today $mtbDir/ncbi/ncbi.latest

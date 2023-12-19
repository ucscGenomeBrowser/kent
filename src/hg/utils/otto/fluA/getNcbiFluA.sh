#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

# Download Influenza A GenBank FASTA and metadata using NCBI Virus query (NCBI Datasets gives
# fasta but no metadata).

today=$(date +%F)

fluADir=/hive/data/outside/otto/fluA

minSize=800

mkdir -p $fluADir/ncbi/ncbi.$today
cd $fluADir/ncbi/ncbi.$today

# Influenza A virus NCBI Taxonomy ID = 11320
taxId=11320

metadataUrl='https://www.ncbi.nlm.nih.gov/genomes/VirusVariation/vvsearch2/?fq=%7B%21tag%3DSeqType_s%7DSeqType_s%3A%28%22Nucleotide%22%29&fq=VirusLineageId_ss%3A%28'$taxId'%29&q=%2A%3A%2A&cmd=download&dlfmt=csv&fl=genbank_accession_rev%3AAccVer_s%2Cisolate%3AIsolate_s%2Cregion%3ARegion_s%2Clocation%3ACountryFull_s%2Ccollected%3ACollectionDate_s%2Csubmitted%3ACreateDate_dt%2Clength%3ASLen_i%2Chost%3AHost_s%2Cbioproject_accession%3ABioProject_s%2Cbiosample_accession%3ABioSample_s%2Csra_accession%3ASRALink_csv%2Ctitle%3ADefinition_s%2Cauthors%3AAuthors_csv%2Cpublications%3APubMed_csv%2Cstrain%3AStrain_s&sort=id+asc&email='$USER'@soe.ucsc.edu'

attempt=0
maxAttempts=5
retryDelay=300
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "metadata attempt $attempt"
    if curl -fSs $metadataUrl | grep -v '^[0-9]' | csvToTab > metadata.tsv; then
        break;
    else
        echo "FAILED metadata; will try again after $retryDelay seconds"
        rm -f metadata.tsv
        sleep $retryDelay
        # Double the delay to give NCBI progressively more time
        retryDelay=$(($retryDelay * 2))
    fi
done
if [[ ! -f metadata.tsv ]]; then
    echo "datasets command failed $maxAttempts times; quitting."
#    exit 1
fi
wc -l metadata.tsv

attempt=0
maxAttempts=5
retryDelay=300
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "fasta attempt $attempt"
    if datasets download virus genome taxon $taxId --include genome,biosample; then
    else
        echo "FAILED fasta; will try again after $retryDelay seconds"
        rm -f ncbi_dataset.zip
        sleep $retryDelay
        # Double the delay to give NCBI progressively more time
        retryDelay=$(($retryDelay * 2))
    fi
done
if [[ ! -s ncbi_dataset.zip ]]; then
    echo "fasta query failed $maxAttempts times; quitting."
#***    exit 1
fi
unzip ncbi_dataset.zip
faFilter -minSize=$minSize ncbi_dataset/data/genomic.fna stdout \
| xz -T 20 > genbank.fa.xz
faSize <(xzcat genbank.fa.xz)

rm -f $fluADir/ncbi/ncbi.latest
ln -s ncbi.$today $fluADir/ncbi/ncbi.latest

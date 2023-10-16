#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

# Download all subtypes (1-4) of dengue virus (taxid 12637) from NCBI.
# assembly              Organism name       NC_            taxid    GenBank
# GCF_000862125.1	Dengue virus 1      NC_001477.1    11053    U88536.1 (clone 45AZ5)
# GCF_000871845.1	Dengue virus 2      NC_001474.2    11060    U87411.1 (Thailand/16681/84)
# GCF_000866625.1	Dengue virus 3      NC_001475.2    11069    AY099336.1 (D3/H/IMTSSA-SRI/2000/1266)
# GCF_000865065.1	Dengue virus 4      NC_002640.1    11070    NC_002640.1 (recombinant clone rDEN4)

today=$(date +%F)

dengueDir=/hive/data/outside/otto/dengue

minSize=9000

mkdir -p $dengueDir/ncbi/ncbi.$today
cd $dengueDir/ncbi/ncbi.$today

# Download all dengue sequences, sort out subtypes later.
taxId=12637

# Thank you Nextstrain (monkeypox/ingest/bin/genbank-url) for query format:
metadataUrl='https://www.ncbi.nlm.nih.gov/genomes/VirusVariation/vvsearch2/?fq=%7B%21tag%3DSeqType_s%7DSeqType_s%3A%28%22Nucleotide%22%29&fq=VirusLineageId_ss%3A%28'$taxId'%29&q=%2A%3A%2A&cmd=download&dlfmt=csv&fl=genbank_accession_rev%3AAccVer_s%2Cisolate%3AIsolate_s%2Cregion%3ARegion_s%2Clocation%3ACountryFull_s%2Ccollected%3ACollectionDate_s%2Csubmitted%3ACreateDate_dt%2Clength%3ASLen_i%2Chost%3AHost_s%2Cbioproject_accession%3ABioProject_s%2Cbiosample_accession%3ABioSample_s%2Csra_accession%3ASRALink_csv%2Ctitle%3ADefinition_s%2Cauthors%3AAuthors_csv%2Cpublications%3APubMed_csv%2Cstrain%3AStrain_s%2Cserotype%3ASerotype_s&sort=id+asc&email='$USER'@soe.ucsc.edu'

attempt=0
maxAttempts=5
retryDelay=300
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "metadata attempt $attempt"
    if curl -fSs $metadataUrl | csvToTab | tawk '$7 >= '$minSize > metadata.tsv; then
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
    exit 1
fi
wc -l metadata.tsv

attempt=0
maxAttempts=5
retryDelay=300
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "fasta attempt $attempt"
    if datasets download virus genome taxon $taxId --include genome,biosample; then
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
    echo "fasta query failed $maxAttempts times; quitting."
    exit 1
fi
unzip ncbi_dataset.zip
faFilter -minSize=$minSize ncbi_dataset/data/genomic.fna stdout \
| xz -T 20 > genbank.fa.xz
faSize <(xzcat genbank.fa.xz)

# Make sure the download wasn't truncated without reporting an error:
count=$(wc -l < metadata.tsv)
minSamples=12000
if (( $count < $minSamples )); then
    echo "*** Too few samples ($count)!  Expected at least $minSamples.  Halting. ***"
    exit 1
fi

rm -f $dengueDir/ncbi/ncbi.latest
ln -s ncbi.$today $dengueDir/ncbi/ncbi.latest

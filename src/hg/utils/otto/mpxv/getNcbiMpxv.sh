#!/bin/bash
source ~/.bashrc
set -beEu -x -o pipefail

# Download MPXV GenBank FASTA and metadata using NCBI Virus query (NCBI Datasets gives fasta
# but no metadata).

today=$(date +%F)

mpxvDir=/hive/data/outside/otto/mpxv

minSize=150000

mkdir -p $mpxvDir/ncbi/ncbi.$today
cd $mpxvDir/ncbi/ncbi.$today

taxId=10244

# Thank you Nextstrain (monkeypox/ingest/bin/genbank-url):
metadataUrl='https://www.ncbi.nlm.nih.gov/genomes/VirusVariation/vvsearch2/?fq=%7B%21tag%3DSeqType_s%7DSeqType_s%3A%28%22Nucleotide%22%29&fq=VirusLineageId_ss%3A%28'$taxId'%29&q=%2A%3A%2A&cmd=download&dlfmt=csv&fl=genbank_accession_rev%3AAccVer_s%2Cisolate%3AIsolate_s%2Cregion%3ARegion_s%2Clocation%3ACountryFull_s%2Ccollected%3ACollectionDate_s%2Csubmitted%3ACreateDate_dt%2Clength%3ASLen_i%2Chost%3AHost_s%2Cbioproject_accession%3ABioProject_s%2Cbiosample_accession%3ABioSample_s%2Csra_accession%3ASRALink_csv%2Ctitle%3ADefinition_s%2Cauthors%3AAuthors_csv%2Cpublications%3APubMed_csv&sort=id+asc&email='$USER'@soe.ucsc.edu'

attempt=0
maxAttempts=5
retryDelay=300
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "metadata attempt $attempt"
    if curl -fSs $metadataUrl | csvToTab \
        | tawk '$7 >= '$minSize' && $1 !~ /^NC_/' \
        | sed -re 's/\tUNVERIFIED: /\t/;' \
        | sed -re 's/\tMonkeypox virus /\t/;' \
        | sed -re 's/\tisolate /\t/;' \
        | sed -re 's/\tstrain /\t/;' \
        | sed -re 's/, (complete|partial) (genome|cds)\t/\t/;' \
        | sed -re 's/\tMPXV[_-]/\t/g;' \
        | sed -re 's@\t(hMPX|hMPXV|hMpxV|MpxV|MPxV|MPXV|MpxV|MPX|Monkeypox|MPXV22)/@\t@g;' \
        | sed -re 's@\t[Hh]uman/@\t@g;' \
        | sed -re 's@RNA genome assembly, complete genome: monopartite@@;' \
        > metadata.tsv; then
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
    echo "metadata query failed $maxAttempts times; quitting."
    exit 1
fi
wc -l metadata.tsv

if [[ ! -s metadata.tsv ]]; then
    echo "metadata query appeared to succeed but gave 0-length output"
    exit 1
fi

attempt=0
maxAttempts=5
retryDelay=300
while [[ $((++attempt)) -le $maxAttempts ]]; do
    echo "fasta attempt $attempt"
    if datasets download virus genome taxon $taxId --include genome,biosample --no-progressbar; then
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

# Extract metadata for only sequences from the 2017 hMPXV outbreak, restoring some missing dates
# (thanks Nextstrain -- monkeypox/ingest/source-data/annotations.tsv)
tawk '{
    if ($1 ~ /^MT903337/) { $5 = "2018"; }
    else if ($1 ~ /^MT903339/) { $5 = "2018"; }
    else if ($1 ~ /^MT903340/) { $5 = "2018"; }
    else if ($1 ~ /^MT903341/) { $5 = "2018-08-14"; }
    else if ($1 ~ /^MT903342/) { $5 = "2019-04-30"; }
    else if ($1 ~ /^MT903342/) { $5 = "2019-05"; }
    else if ($1 ~ /^MT903343/) { $5 = "2018-09"; }
    else if ($1 ~ /^MT903344/) { $5 = "2018-09"; }
    else if ($1 ~ /^MT903345/) { $5 = "2018-09"; }
    print;
}' metadata.tsv \
| tawk '$5 ~ /^20(1[7-9]|2)/ &&
        $4 != "Central African Republic" && $4 != "'"Cote d'Ivoire"'" &&
        ($8 == "Homo sapiens" || $8 == "")' \
    > metadata.2017outbreak.tsv
wc -l metadata.2017outbreak.tsv

# Run nextclade on the lot (takes only a few seconds as of 2022-07-25) to get alignments,
# QC, clade assignments.
# Clade II 2017 outbreak:
nextclade dataset get --name hMPXV --output-zip hMPXV.zip
time nextclade run \
    -D hMPXV.zip \
    -j 30 \
    --output-tsv nextclade.II.tsv.gz \
    --output-fasta nextalign.II.fa.xz \
    --retry-reverse-complement true \
    genbank.fa.xz

# Clade I: Nextstrain uses a different reference (DQ011155.1 Zaire_1979-005 instead of
# RefSeq NC_003310.1 AF380138.1 Zaire-96-I-16); use the Nextstrain dataset for clade assignments,
# but run nextclade separately with the RefSeq sequence for sequence alignments.
nextclade dataset get --name nextstrain/mpox/clade-i --output-zip mpox_clade-i.zip
nextclade run \
    -D mpox_clade-i.zip \
    -j 30 \
    --output-tsv nextclade.I.tsv.gz \
    --retry-reverse-complement true \
    genbank.fa.xz

# Use alignment parameters from pathogen.json in mpox_clade-i.zip
nextclade run \
    --input-ref $mpxvDir/GCF_000857045.1.NC_003310.1.fa \
    --excess-bandwidth 100 \
    --terminal-bandwidth 300 \
    --allowed-mismatches 8 \
    --window-size 40 \
    --min-seed-cover 0.1 \
    --output-fasta nextalign.I.fa.xz \
    genbank.fa.xz

# Extract metadata for only clade I sequences
zcat nextclade.I.tsv.gz \
| tawk '$3 ~ /^I[^I]/ {print $2;}' \
| grep -Fwf - metadata.tsv \
    > metadata.cladeI.tsv


rm -f $mpxvDir/ncbi/ncbi.latest
ln -s ncbi.$today $mpxvDir/ncbi/ncbi.latest

#!/bin/bash
set -beEu -o pipefail

WORKDIR=$1

cleanUpOnError () {
    echo "orphanet build failed, work directory:"
    echo "$WORKDIR"
} 

trap cleanUpOnError ERR
trap "cleanUpOnError; exit 1" SIGINT SIGTERM
umask 002

if [ ! -d "${WORKDIR}" ]; then
    printf "ERROR in orphanet build, can not find the directory: %s\n" "${WORKDIR}"
    exit 255
fi

# the release directory, where gbdb symlinks will point
if [ ! -d release ]; then
    mkdir -p ${WORKDIR}/release/{hg19,hg38}
fi

cd $WORKDIR
today=$(date +%F)

# orphanet updates every month so just make the new dir and download
mkdir $today && cd $today
wget --quiet http://www.orphadata.org/data/xml/en_product6.xml
wget --quiet http://www.orphadata.org/data/xml/en_product4.xml
wget --quiet http://www.orphadata.org/data/xml/en_product9_prev.xml
wget --quiet http://www.orphadata.org/data/xml/en_product9_ages.xml

# turn the xmls into one bed per assembly
# Note that this script relies on $db.ensGene.gtf files existing
# in $WORKDIR. You can generate these files like so:
# cp /usr/local/apache/htdocs-hgdownload/goldenPath/hg19/bigZips/genes/hg19.ensGene.gtf.gz .
# cp /usr/local/apache/htdocs-hgdownload/goldenPath/hg38/bigZips/genes/hg38.ensGene.gtf.gz .
# gzip -d hg19.ensGene.gtf.gz 
# gzip -d hg38.ensGene.gtf.gz
../parseOrphadata.py

# Sort both files
sort -k1,1 -k2,2n orphadata.hg19.bed > sortedOrphadata.hg19.bed
sort -k1,1 -k2,2n orphadata.hg38.bed > sortedOrphadata.hg38.bed

# Check not too much has changed
oldHg19Count=$(bigBedInfo /gbdb/hg19/bbi/orphanet/orphadata.bb | grep itemCount | cut -d' ' -f2 | sed 's/,//g')
newHg19Count=$(wc -l sortedOrphadata.hg19.bed | cut -d' ' -f1)
oldHg38Count=$(bigBedInfo /gbdb/hg38/bbi/orphanet/orphadata.bb | grep itemCount | cut -d' ' -f2 | sed 's/,//g')
newHg38Count=$(wc -l sortedOrphadata.hg38.bed | cut -d' ' -f1)
echo $oldHg19Count $newHg19Count | awk '{diff=(($2-$1)/$1); if (diff > 0.1 || diff < -0.1) {printf "validate on hg19 Orphanet failed: old count: %d, new count: %d, difference: %0.2f\n", $1,$2,diff; exit 1;}}'
echo $oldHg38Count $newHg38Count | awk '{diff=(($2-$1)/$1); if (diff > 0.1 || diff < -0.1) {printf "validate on hg38 Orphanet failed: old count: %d, new count: %d, difference: %0.2f\n", $1,$2,diff; exit 1;}}'

# Make bigBed files
bedToBigBed -tab -as=$WORKDIR/orphadata.as -type=bed9+20 sortedOrphadata.hg19.bed /hive/data/genomes/hg19/chrom.sizes orphadata.hg19.bb
bedToBigBed -tab -as=$WORKDIR/orphadata.as -type=bed9+20 sortedOrphadata.hg38.bed /hive/data/genomes/hg38/chrom.sizes orphadata.hg38.bb

cp orphadata.hg19.bb $WORKDIR/release/hg19/orphadata.bb
cp orphadata.hg38.bb $WORKDIR/release/hg38/orphadata.bb

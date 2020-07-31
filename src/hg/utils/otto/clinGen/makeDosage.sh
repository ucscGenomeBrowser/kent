#!/bin/bash

# downloads and builds the curated ClinGen Curated CNVS track
# assumes running in the build directory:
# /hive/data/outside/otto/clinGen/
set -beEu -o pipefail

WORKDIR=$1
mkdir -p ${WORKDIR}/clinGenDosage
cd ${WORKDIR}/clinGenDosage

echo "user anonymous
ls ClinGen*
bye" > ftp.dosage.cmds

if [ -e release.list ]
then
    mv release.list prev.release.list
fi
touch prev.release.list
rm -f release.list

# connect and list a directory, result to file: ls.check
ftp -n -v -i ftp.clinicalgenome.org 2>&1 < ftp.dosage.cmds &> ls.check
grep "haplo\|triplo\|curation" ls.check | sort > release.list || echo "Error - no bed files found"

# see if anything is changing, if so, notify, download, and build
diff prev.release.list release.list > release.diff || true
count=`wc -l release.diff | cut -d' ' -f1`
if [ "${count}" -gt 1 ]
then
    echo "New ClinGen Dosage update"
    today=`date +%F`
    mkdir -p ${today}/{download,output}
    cd ${today}/download
    for grc in GRCh37 GRCh38
    do
        wget -N -q "ftp://ftp.clinicalgenome.org/ClinGen_region_curation_list_${grc}.tsv"
        wget -N -q "ftp://ftp.clinicalgenome.org/ClinGen_gene_curation_list_${grc}.tsv"
        if [ ${grc} == "GRCh37" ]
        then
            echo $grc
            # the process script creates a haplo.bed and triplo.bed file:
            ../../../processClinGenDosage.py ClinGen_region_curation_list_${grc}.tsv ClinGen_gene_curation_list_${grc}.tsv ../output/hg19
            sort -k1,1 -k2,2n ../output/hg19.haplo.bed > ../output/hg19.clinGenHaplo.bed
            sort -k1,1 -k2,2n ../output/hg19.triplo.bed > ../output/hg19.clinGenTriplo.bed
            bedToBigBed -type=bed9+13 -as=../../clinGenDosageHaplo.as -tab ../output/hg19.clinGenHaplo.bed /hive/data/genomes/hg19/chrom.sizes ../output/hg19.clinGenHaplo.bb
            bedToBigBed -type=bed9+13 -as=../../clinGenDosageTriplo.as -tab ../output/hg19.clinGenTriplo.bed /hive/data/genomes/hg19/chrom.sizes ../output/hg19.clinGenTriplo.bb
            cp ../output/hg19.clinGenHaplo.bb ${WORKDIR}/release/hg19/clinGenHaplo.bb
            cp ../output/hg19.clinGenTriplo.bb ${WORKDIR}/release/hg19/clinGenTriplo.bb
        elif [ ${grc} == "GRCh38" ]
        then
            echo $grc
            # the process script creates a haplo.bed and triplo.bed file:
            ../../../processClinGenDosage.py ClinGen_region_curation_list_${grc}.tsv ClinGen_gene_curation_list_${grc}.tsv ../output/hg38
            sort -k1,1 -k2,2n ../output/hg38.haplo.bed > ../output/hg38.clinGenHaplo.bed
            sort -k1,1 -k2,2n ../output/hg38.triplo.bed > ../output/hg38.clinGenTriplo.bed
            bedToBigBed -type=bed9+13 -as=../../clinGenDosageHaplo.as -tab ../output/hg38.clinGenHaplo.bed /hive/data/genomes/hg38/chrom.sizes ../output/hg38.clinGenHaplo.bb
            bedToBigBed -type=bed9+13 -as=../../clinGenDosageTriplo.as -tab ../output/hg38.clinGenTriplo.bed /hive/data/genomes/hg38/chrom.sizes ../output/hg38.clinGenTriplo.bb
            cp ../output/hg38.clinGenHaplo.bb ${WORKDIR}/release/hg38/clinGenHaplo.bb
            cp ../output/hg38.clinGenTriplo.bb ${WORKDIR}/release/hg38/clinGenTriplo.bb
        fi
    done
    cd ../..
else
    echo "No ClinGen CNV update"
fi

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
    for db in hg19 hg38
    do
        grc=""
        if [ ${db} == "hg19" ]
        then
            grc="GRCh37"
            wget -N -q "ftp://ftp.clinicalgenome.org/ClinGen_region_curation_list_${grc}.tsv"
            wget -N -q "ftp://ftp.clinicalgenome.org/ClinGen_gene_curation_list_${grc}.tsv"
        elif [ ${db} == "hg38" ]
        then
            grc="GRCh38"
            wget -N -q "ftp://ftp.clinicalgenome.org/ClinGen_region_curation_list_${grc}.tsv"
            wget -N -q "ftp://ftp.clinicalgenome.org/ClinGen_gene_curation_list_${grc}.tsv"
        fi
        echo $grc
        # the process script creates a haplo.bed and triplo.bed file:
        hgsql -Ne "select phenotypeId, description from omimPhenotype" ${db} > ${db}.omimPhenotypes
        ../../../processClinGenDosage.py ClinGen_region_curation_list_${grc}.tsv ClinGen_gene_curation_list_${grc}.tsv ${db}.omimPhenotypes ../output/${db}
        sort -k1,1 -k2,2n ../output/${db}.haplo.bed > ../output/${db}.clinGenHaplo.bed
        sort -k1,1 -k2,2n ../output/${db}.triplo.bed > ../output/${db}.clinGenTriplo.bed
        bedToBigBed -type=bed9+16 -as=../../clinGenDosageHaplo.as -tab ../output/${db}.clinGenHaplo.bed /hive/data/genomes/${db}/chrom.sizes ../output/${db}.clinGenHaplo.bb
        bedToBigBed -type=bed9+16 -as=../../clinGenDosageTriplo.as -tab ../output/${db}.clinGenTriplo.bed /hive/data/genomes/${db}/chrom.sizes ../output/${db}.clinGenTriplo.bb
        cp ../output/${db}.clinGenHaplo.bb ${WORKDIR}/release/${db}/clinGenHaplo.bb
        cp ../output/${db}.clinGenTriplo.bb ${WORKDIR}/release/${db}/clinGenTriplo.bb
    done
    cd ../..
else
    echo "No ClinGen CNV update"
fi

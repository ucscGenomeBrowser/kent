#!/bin/bash

# downloads and builds the curated ClinGen Curated CNVS track
# assumes running in the build directory:
# /hive/data/outside/otto/clinGen/
set -beEu -o pipefail

WORKDIR=$1
mkdir -p ${WORKDIR}/clinGenDosage
cd ${WORKDIR}/clinGenDosage

echo "user anonymous
ls ClinGen*curation_list*
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
        ../../../processClinGenDosage.py ClinGen_region_curation_list_${grc}.tsv ClinGen_gene_curation_list_${grc}.tsv ../output/${db}
        sort -k1,1 -k2,2n ../output/${db}.haplo.bed > ../output/${db}.clinGenHaplo.bed
        sort -k1,1 -k2,2n ../output/${db}.triplo.bed > ../output/${db}.clinGenTriplo.bed

        # validate new release by diffing against old release:
        bigBedToBed ${WORKDIR}/release/${db}/clinGenHaplo.bb ${db}.old.haplo.bed
        bigBedToBed ${WORKDIR}/release/${db}/clinGenTriplo.bb ${db}.old.triplo.bed
        oldHaploLc=$(wc -l ${db}.old.haplo.bed | cut -d' ' -f1)
        newHaploLc=$(wc -l ../output/${db}.clinGenHaplo.bed | cut -d' ' -f1)
        # diff returns 1 when it finds a difference, which causes the script to silently exit. Add the
        # '|| true' to prevent that behavior since we want the number of diffs
        diffCountHaplo=$( (diff ${db}.old.haplo.bed ../output/${db}.clinGenHaplo.bed --speed-large-files || true) | grep -c '^[\>\<]')
        diffPerc=$(echo $oldHaploLc $newHaploLc | awk '{diff=(($2-$1)/$1); printf "%0.2f", diff}')
        shouldError=$(echo $diffPerc | awk '{if ($1 > 0.1 || $1 < -0.1) {print 0} else {print 1}}')
        if [[ $shouldError -eq 0 ]]; then
            printf "validate on %s ClinGen Haplo failed: old count: %d, new count: %d, difference: %0.2f\n" $db $oldHaploLc $newHaploLc $diffPerc
            exit 1
        fi
        diffPerc=$(echo $newHaploLc $diffCountHaplo | awk '{diff=$2/$1; printf "%0.2f", diff}')
        shouldError=$(echo $diffPerc | awk '{if ($1 > 0.1 || $1 < -0.1) {print 0} else {print 1}}')
        if [[ $shouldError -eq 0 ]]; then
            printf "validate on %s ClinGen Haplo failed with too many differences to old version: %d lines changed, new count: %d, difference: %0.2f\n" $db $diffCountHaplo $newHaploLc $diffPerc
            exit 1
        fi
        oldTriploLc=$(wc -l ${db}.old.triplo.bed | cut -d' ' -f1)
        newTriploLc=$(wc -l ../output/${db}.clinGenTriplo.bed | cut -d' ' -f1)
        # diff returns 1 when it finds a difference, which causes the script to silently exit. Add the
        # '|| true' to prevent that behavior since we want the number of diffs
        diffCountTriplo=$( (diff ${db}.old.triplo.bed ../output/${db}.clinGenTriplo.bed --speed-large-files || true) | grep -c '^[\>\<]')
        diffPerc=$(echo $oldTriploLc $newTriploLc | awk '{diff=(($2-$1)/$1); printf "%0.2f", diff}')
        shouldError=$(echo $diffPerc | awk '{if ($1 > 0.1 || $1 < -0.1) {print 0} else {print 1}}')
        if [[ $shouldError -eq 0 ]]; then
            printf "validate on %s ClinGen Triplo failed: old count: %d, new count: %d, difference: %0.2f\n" $db $oldTriploLc $newTriploLc $diffPerc
            exit 1
        fi
        diffPerc=$(echo $newTriploLc $diffCountTriplo | awk '{diff=$2/$1; printf "%0.2f", diff}')
        shouldError=$(echo $diffPerc | awk '{if ($1 > 0.1 || $1 < -0.1) {print 0} else {print 1}}')
        if [[ $shouldError -eq 0 ]]; then
            printf "validate on %s ClinGen Triplo failed with too many differences to old version: %d lines changed, new count: %d, difference: %0.2f\n" $db $diffCountTriplo $newTriploLc $diffPerc
            exit 1
        fi

        bedToBigBed -type=bed9+17 -as=../../clinGenDosageHaplo.as -tab ../output/${db}.clinGenHaplo.bed /hive/data/genomes/${db}/chrom.sizes ../output/${db}.clinGenHaplo.bb
        bedToBigBed -type=bed9+17 -as=../../clinGenDosageTriplo.as -tab ../output/${db}.clinGenTriplo.bed /hive/data/genomes/${db}/chrom.sizes ../output/${db}.clinGenTriplo.bb
        cp ../output/${db}.clinGenHaplo.bb ${WORKDIR}/release/${db}/clinGenHaplo.bb
        cp ../output/${db}.clinGenTriplo.bb ${WORKDIR}/release/${db}/clinGenTriplo.bb
    done
    cd ../..
else
    echo "No ClinGen CNV update"
fi

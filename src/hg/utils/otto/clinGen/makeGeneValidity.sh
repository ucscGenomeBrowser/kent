#!/bin/bash

# downloads and builds the ClinGen Gene Validity track
# assumes running in the build directory:
# /hive/data/outside/otto/clinGen/
set -beEu -o pipefail

WORKDIR=$1
mkdir -p ${WORKDIR}/clinGenGeneValidity
cd ${WORKDIR}/clinGenGeneValidity

# the release directory, where gbdb symlinks will point
if [ ! -d release ]; then
    mkdir -p ${WORKDIR}/release/{hg19,hg38}
fi

echo "user anonymous
ls hg*ClinGenBigBed.bb
bye" > ftp.geneDiseaseValidity.cmds

if [ -e release.list ]
then
    mv release.list prev.release.list
fi
touch prev.release.list
rm -f release.list

# connect and list a directory, result to file: ls.check
ftp -n -v -i ftp.clinicalgenome.org 2>&1 < ftp.geneDiseaseValidity.cmds &> ls.check
grep "hg.*ClinGenBigBed.bb" ls.check | sort > release.list || echo "Error - no bigBed files found"

# see if anything is changing, if so, notify, download, and build
diff prev.release.list release.list > release.diff || true
count=`wc -l release.diff | cut -d' ' -f1`
if [ "${count}" -gt 1 ]; then
    printf "New ClinGen Gene-Disease tracks\n"
    today=`date +%F`
    mkdir -p $today
    cd $today
    wget -N -q "ftp://ftp.clinicalgenome.org/hg19ClinGenBigBed.bb"
    wget -N -q "ftp://ftp.clinicalgenome.org/hg38ClinGenBigBed.bb"
    cp hg19ClinGenBigBed.bb ${WORKDIR}/release/hg19/clinGenGeneDisease.bb
    cp hg38ClinGenBigBed.bb ${WORKDIR}/release/hg38/clinGenGeneDisease.bb
    cd ..
    echo "ClinGen Gene-Disease update done: `date`"
else
    echo "No update"
fi

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

#	see if anything is changing, if so, email notify, download, and build
wget -q https://raw.githubusercontent.com/RedRiver1816/ClinGen_UCSC_Hub/master/hg19/hg19ClinGenBigBed.bb -O tempUpdate
if [[ ! -e lastUpdate || tempUpdate -nt lastUpdate ]]; then
    printf "New ClinGen Gene-Disease tracks hub:\nhttps://github.com/RedRiver1816/ClinGen_UCSC_Hub\n"
    today=`date +%F`
    mkdir -p $today
    cd $today
    hubClone -download https://raw.githubusercontent.com/RedRiver1816/ClinGen_UCSC_Hub/master/hub.txt
    cp ClinGenHub/hg19/hg19ClinGenBigBed.bb ${WORKDIR}/release/hg19/clinGenGeneDisease.bb
    cp ClinGenHub/hg38/hg38ClinGenBigBed.bb ${WORKDIR}/release/hg38/clinGenGeneDisease.bb
    cd ..
    mv tempUpdate lastUpdate
    echo "ClinGen Gene-Disease update done: `date`"
else
    rm tempUpdate
    echo "No update"
fi

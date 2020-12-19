#!/bin/bash

# Do not modify this script, modify the source tree copy:
# src/hg/utils/dbVar/checkClinGen.sh
# This script is used via a cron job and kept in $HOME/bin/scripts/

set -beEu -o pipefail
WORKDIR=$1

cleanUpOnError () {
    echo "ClinGen build failed"
} 

trap cleanUpOnError ERR
trap "cleanUpOnError;" SIGINT SIGTERM

# cron jobs need to ensure this is true
umask 002

# this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    printf "ERROR in ClinGen build, can not find the directory: %s\n" "${WORKDIR}"
    exit 255
fi

# setup the release directory, which holds all the current bigBeds:
mkdir -p ${WORKDIR}/release/{hg19,hg38}

# There are multiple parts to this otto job, each one runs separately and does it's own
# checking for updated files and stuff. Each downloads data from a separate website which
# can go down so don't force the others to die just because one is having a problem.
set +e
./makeCnv.sh ${WORKDIR}
./makeDosage.sh ${WORKDIR}
#./makeEvRepo.sh ${WORKDIR}
./makeGeneValidity.sh ${WORKDIR}
set -e

echo "ClinGen update done."

#!/bin/bash

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/dbVar/checkDbVar.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

set -beEu -o pipefail
WORKDIR=$1

cleanUpOnError () {
    echo "dbVar build failed"
} 

trap cleanUpOnError ERR
trap "cleanUpOnError; exit 1" SIGINT SIGTERM
umask 002

#	cron jobs need to ensure this is true

#	this is where we are going to work 
if [ ! -d "${WORKDIR}" ]; then
    printf "ERROR in dbVar build, can not find the directory: %s\n" "${WORKDIR}"
    exit 255
fi

# the release directory, where gbdb symlinks will point
if [ ! -d release ]; then
    mkdir -p ${WORKDIR}/release/{hg19,hg38}
fi

cd "${WORKDIR}"

# check if genome in a bottle variants have updated:
./checkNstd175.sh ${WORKDIR}

#	see if anything is changing, if so, email notify, download, and build
wget -q https://ftp.ncbi.nlm.nih.gov/pub/dbVar/sandbox/dbvarhub/hub.txt -O tempUpdate
if [[ ! -e lastUpdate || tempUpdate -nt lastUpdate ]]; then
    printf "New dbVar track hub:\nhttps://ftp.ncbi.nlm.nih.gov/pub/dbVar/sandbox/dbvarhub/\n"
    today=`date +%F`
    mkdir -p $today
    cd $today
    hubClone -download https://ftp.ncbi.nlm.nih.gov/pub/dbVar/sandbox/dbvarhub/hub.txt
    cp dbVar/hg19/common*.bb ../release/hg19/
    cp dbVar/hg19/conflict*.bb ../release/hg19/
    cp dbVar/hg38/common*.bb ../release/hg38/
    cp dbVar/hg38/conflict*.bb ../release/hg38/
    cd ..
    mv tempUpdate lastUpdate
    echo "dbVar hub update done: `date`"
else
    rm tempUpdate
    echo "No update"
fi


#!/bin/sh -e
set -eEu -o pipefail
#	Do not modify this script, modify the source tree copy:
#	src/utils/decipher/checkDecipher.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

#	cron jobs need to ensure this is true
#       current login requires the user be braney
umask 002

trap reportErr ERR

function reportErr
{
    echo "ERROR: DECIPHER pipeline failed"
    exit 1;
}


WORKDIR="/hive/data/outside/otto/decipher"
export WORKDIR

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in DECIPHER release watch, Can not find the directory:
    ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"
curl --silent -K curl.config -o decipher-variants-grch38.bed

if [ ! -s decipher-variants-grch38.md5 ] || ! md5sum -c --status decipher-variants-grch38.md5
then
    mkdir -p ${WORKDIR}/release/hg38

    today=`date +%F`
    mkdir -p $today
    cp -p decipher-variants-grch38.bed $today
    cd $today

    # build the new DECIPHER track tables (builds bigBed for cnv's)
    ../buildDecipher decipher-variants-grch38.bed
    ../validateDecipher.sh

    # now install
    for db in hg38
    do
        for i in `cat ../decipher.tables`
        do 
        n=$i"New"
        o=$i"Old"
        hgsqlSwapTables $db $n $i $o -dropTable3
        done
    done
    cp -p --remove-destination decipherCnv.bb ../release/hg38/decipherCnv.bb

    echo "DECIPHER Installed `date`" 

    cd ${WORKDIR}
    # Update our md5sum record
    md5sum decipher-variants-grch38.bed > decipher-variants-grch38.md5
#Commenting out heartbeat message below so as not to get spam.
#else
#    echo "No update"
fi

rm decipher-variants-grch38.bed

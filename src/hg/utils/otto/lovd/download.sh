#!/bin/sh -e
#	Do not modify this script, modify the source tree copy:
#	src/utils/lovd/check.sh
#	This script is used via a cron job 

#	cron jobs need to ensure this is true
umask 002

WORKDIR="/hive/data/outside/otto/lovd"
#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in lovd release, Can not find the directory: ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"

today=`date +%F`
mkdir -p $today
cd $today

# http request needs to come from hgwdev IP address otherwise file not found error
for db in hg38 hg19
do
    curl -s 'https://varcache.lovd.nl/bed/'${db}'?add_id_ncbi=1&add_annotation=1' | \
        grep -v track | grep -v ^$ | sed -e s/^/chr/g | \
        tawk '{gsub(";",FS,$6); gsub("effect:|lovd_count:","",$6); print }' | \
        sort -k1,1 -k2,2n > lovd.${db}.bed
        lcount=`wc -l lovd.${db}.bed |  cut -d' ' -f1 `
        if [ $lcount -lt "10000" ] ; then
                echo LOVD file lovd.${db}.bed is too small
                exit 1
        fi
done

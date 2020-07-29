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
    wget -q 'http://varcache.lovd.nl/bed/'${db}'?add_id_ncbi=1&add_annotation=1' -O - | grep -v track | grep -v ^$ > lovd.${db}.bed
    sed -i s/^/chr/g lovd.${db}.bed
    sed -i 's/effect:/Variant Effect: /g' lovd.${db}.bed
    sed -i 's/;lovd_count:/<br>Number of LOVD Installations reporting this variant: /g' lovd.${db}.bed
done

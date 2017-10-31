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
wget -q 'http://varcache.lovd.nl/bed/hg19?add_id_ncbi=1&add_annotation=1' -O - | grep -v track | grep -v ^$ > lovd.hg19.bed
wget -q 'http://varcache.lovd.nl/bed/hg18?add_id_ncbi=1&add_annotation=1' -O - | grep -v track | grep -v ^$ > lovd.hg18.bed

sed -i s/^/chr/g lovd.hg19.bed
sed -i s/^/chr/g lovd.hg18.bed

# effect:;lovd_count:1
sed -i 's/effect:/Variant Effect: /g' lovd.hg19.bed
sed -i 's/effect:/Variant Effect: /g' lovd.hg18.bed

sed -i 's/;lovd_count:/<br>Number of LOVD Installations reporting this variant: /g' lovd.hg19.bed
sed -i 's/;lovd_count:/<br>Number of LOVD Installations reporting this variant: /g' lovd.hg18.bed

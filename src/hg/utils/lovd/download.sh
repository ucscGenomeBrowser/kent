#!/bin/sh -ex
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
wget http://varcache.lovd.nl/bed/hg19 -O - | grep -v track | grep -v ^$ > lovd.hg19.bed
wget http://varcache.lovd.nl/bed/hg18 -O - | grep -v track | grep -v ^$ > lovd.hg18.bed

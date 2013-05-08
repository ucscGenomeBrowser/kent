#!/bin/sh -ex
#	Do not modify this script, modify the source tree copy:
#	src/utils/lovd/load.sh
#	This script is used via a cron job 

#	cron jobs need to ensure this is true
umask 002

WORKDIR="/hive/data/outside/otto/lovd"
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in lovd release, Can not find the directory: ${WORKDIR}" 
    exit 255
fi

today=`date +%F`
cd $today

# bedDetail4.sql was generated like this:
# egrep -v 'score|strand|thick|reserved|block|chromStarts' /cluster/home/max/kent/src/hg/lib/bedDetail.sql > bedDetail4.sql 
hgLoadBed hg19 lovd lovd.hg19.bed -tab -sqlTable=../bedDetail4.sql -renameSqlTable -noBin
hgLoadBed hg18 lovd lovd.hg18.bed -tab -sqlTable=../bedDetail4.sql -renameSqlTable -noBin

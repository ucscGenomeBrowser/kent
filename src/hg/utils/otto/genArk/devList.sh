#!/bin/bash

#############################################################################
###  This source is from the source tree:
###     ~/kent/src/hg/utils/otto/genArk/devList.sh
###  do *not* edit this in the otto directory /hive/data/inside/GenArk/pushRR/
###  where this is used.
###  Running as the otto cron job:
###  58 20 * * * /hive/data/inside/GenArk/pushRR/devList.sh
###
###  This script takes about 90 minutes to run as of 2025-07-30
###
#############################################################################

set -beEu -o pipefail

export DS=`date "+%F"`
export Y=`date "+%Y"`
export M=`date "+%m"`
export machName=`uname -n | cut -d'.' -f1`
export fileOut="gbdbGenArk.${machName}.${DS}"
export statList="gbdbGenArkStat.${machName}.${DS}"
export quickLiftStat="gbdbQuickLiftStat.${machName}.${DS}"
export liftOverStat="gbdbLiftOverStat.${machName}.${DS}"

find -L /gbdb/genark/GCA /gbdb/genark/GCF -type d | sort \
  | gzip -c > /dev/shm/${fileOut}.gz

mkdir -p /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}
cp -p /dev/shm/${fileOut}.gz /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/

rm -f /dev/shm/${fileOut}.gz

cd /gbdb/genark
find -L ./GCA ./GCF -type f | sed -e 's#^./##;' | sort | gzip -c \
	> /dev/shm/gbdbGenark.fl.gz
zcat /dev/shm/gbdbGenark.fl.gz | xargs stat -L --printf="%Y\t%n\n" \
	| gzip -c > /dev/shm/${statList}.gz
cp -p /dev/shm/${statList}.gz /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/
cp -p /dev/shm/${statList}.gz /hive/data/inside/GenArk/pushRR/dev.todayList.gz

rm -f /dev/shm/gbdbGenark.fl.gz /dev/shm/${statList}.gz

cd /gbdb
ls -d */quickLift | while read Q
do
  find -L ./${Q} -type f
done | sed -e 's#^./##;' | sort | gzip -c > /dev/shm/gbdbQuickLift.fl.gz

zcat /dev/shm/gbdbQuickLift.fl.gz | xargs stat -L --printf="%Y\t%n\n" \
  | gzip -c > /dev/shm/${quickLiftStat}.gz

cp -p /dev/shm/${quickLiftStat}.gz /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/
cp -p /dev/shm/${quickLiftStat}.gz /hive/data/inside/GenArk/pushRR/dev.today.quickLiftList.gz

rm -f /dev/shm/gbdbQuickLift.fl.gz /dev/shm/${quickLiftStat}.gz

ls -d */liftOver | while read Q
do
  find -L ./${Q} -type f
done | sed -e 's#^./##;' | sort | gzip -c > /dev/shm/gbdbLiftOver.fl.gz

zcat /dev/shm/gbdbLiftOver.fl.gz | xargs stat -L --printf="%Y\t%n\n" \
  | gzip -c > /dev/shm/${liftOverStat}.gz

cp -p /dev/shm/${liftOverStat}.gz /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/
cp -p /dev/shm/${liftOverStat}.gz /hive/data/inside/GenArk/pushRR/dev.today.liftOverList.gz

rm -f /dev/shm/gbdbLiftOver.fl.gz /dev/shm/${liftOverStat}.gz

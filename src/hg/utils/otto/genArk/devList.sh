#!/bin/bash

set -beEu -o pipefail

export DS=`date "+%F"`
export Y=`date "+%Y"`
export M=`date "+%m"`
export machName=`uname -n | cut -d'.' -f1`
export fileOut="gbdbGenArk.${machName}.${DS}"
export statList="gbdbGenArkStat.${machName}.${DS}"

find -L /gbdb/genark/GCA /gbdb/genark/GCF -type d | sort \
  | gzip -c > /dev/shm/${fileOut}.gz

mkdir -p /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}
cp -p /dev/shm/${fileOut}.gz /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/

rm -f /dev/shm/${fileOut}.gz

cd /gbdb/genark
find -L ./GCA ./GCF -type f | sed -e 's#^./##;' | gzip -c \
	> /dev/shm/gbdbGenark.fl.gz
zcat /dev/shm/gbdbGenark.fl.gz | xargs stat -L --printf="%Y\t%n\n" \
	| gzip -c > /dev/shm/${statList}.gz
cp -p /dev/shm/${statList}.gz /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/
cp -p /dev/shm/${statList}.gz /hive/data/inside/GenArk/pushRR/dev.todayList.gz

rm -f /dev/shm/gbdbGenark.fl.gz /dev/shm/${statList}.gz


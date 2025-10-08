#!/bin/bash

export DS=`date "+%F"`
export Y=`date "+%Y"`
export M=`date "+%m"`
export machName=`uname -n | cut -d'.' -f1`
export directoryList="gbdbGenArk.${machName}.${DS}"
export statList="gbdbGenArkStat.${machName}.${DS}"
export fileList="gbdbGenark.fl"

find -L /gbdb/genark/GCA /gbdb/genark/GCF -type d | sort \
  | gzip -c > /dev/shm/${directoryList}.gz

ssh otto@hgwdev "mkdir -p /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}"
scp -p /dev/shm/${directoryList}.gz \
  otto@hgwdev:/hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/ > /dev/null

rm -f /dev/shm/${directoryList}.gz

cd /gbdb/genark
find . -type f | sed -e 's#^./##;' | gzip -c > /dev/shm/${fileList}.gz
zcat /dev/shm/${fileList}.gz | xargs stat --printf="%Y\t%n\n" | gzip -c > /dev/shm/${statList}.gz
scp -p /dev/shm/${statList}.gz \
  otto@hgwdev:/hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/ > /dev/null

if [[ "${machName}" =~ ^(hgw1|hgwbeta)$ ]]; then
  scp -p /dev/shm/${statList}.gz \
    otto@hgwdev:/hive/data/inside/GenArk/pushRR/${machName}.todayList.gz > /dev/null
fi

rm -f /dev/shm/${fileList}.gz /dev/shm/${statList}.gz

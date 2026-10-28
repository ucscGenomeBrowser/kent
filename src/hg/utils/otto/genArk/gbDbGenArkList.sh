#!/bin/bash

export DS=`date "+%F"`
export Y=`date "+%Y"`
export M=`date "+%m"`
export machName=`uname -n | cut -d'.' -f1`
export directoryList="gbdbGenArk.${machName}.${DS}"
export statList="gbdbGenArkStat.${machName}.${DS}"
export fileList="gbdbGenark.fl"
export quickLiftFileList="gbdbQuickLift.fl"
export quickLiftStat="gbdbQuickLiftStat.${machName}.${DS}"
export liftOverStat="gbdbLiftOverStat.${machName}.${DS}"

##############################################################################
### scp a file into place on hgwdev for readers there: send it to a
### temporary name next to the destination, then rename it, so a reader
### never sees a partly written file
##############################################################################
function scpAtomic() {
  local src="${1}"
  local dest="${2}"
  local tmpDest="${dest}.tmp.$$"
  scp -p "${src}" "otto@hgwdev:${tmpDest}" > /dev/null \
    && ssh otto@hgwdev "mv -f '${tmpDest}' '${dest}'" < /dev/null
}

find -L /gbdb/genark/GCA /gbdb/genark/GCF -type d | sort \
  | gzip -c > /dev/shm/${directoryList}.gz

ssh otto@hgwdev "mkdir -p /hive/data/inside/GenArk/pushRR/logs/${Y}/${M}"
scp -p /dev/shm/${directoryList}.gz \
  otto@hgwdev:/hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/ > /dev/null

rm -f /dev/shm/${directoryList}.gz

cd /gbdb/genark
find . -type f | sed -e 's#^./##;' | sort | gzip -c > /dev/shm/${fileList}.gz
zcat /dev/shm/${fileList}.gz | xargs stat --printf="%Y\t%n\n" | gzip -c > /dev/shm/${statList}.gz
scp -p /dev/shm/${statList}.gz \
  otto@hgwdev:/hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/ > /dev/null

cd /gbdb
ls -d */quickLift | while read Q
do
  find -L ./${Q} -type f
done | sed -e 's#^./##;' | sort | gzip -c > /dev/shm/${quickLiftFileList}.gz

zcat /dev/shm/${quickLiftFileList}.gz | xargs stat -L --printf="%Y\t%n\n" \
  | gzip -c > /dev/shm/${quickLiftStat}.gz

scp -p /dev/shm/${quickLiftStat}.gz \
  otto@hgwdev:/hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/ > /dev/null

ls -d */liftOver | while read Q
do
  find -L ./${Q} -type f
done | sed -e 's#^./##;' | sort | gzip -c > /dev/shm/${liftOverFileList}.gz

zcat /dev/shm/${liftOverFileList}.gz | xargs stat -L --printf="%Y\t%n\n" \
  | gzip -c > /dev/shm/${liftOverStat}.gz

scp -p /dev/shm/${liftOverStat}.gz \
  otto@hgwdev:/hive/data/inside/GenArk/pushRR/logs/${Y}/${M}/ > /dev/null

if [[ "${machName}" =~ ^(hgw1|hgwbeta)$ ]]; then
  scpAtomic /dev/shm/${statList}.gz \
    /hive/data/inside/GenArk/pushRR/${machName}.todayList.gz
  scpAtomic /dev/shm/${quickLiftStat}.gz \
    /hive/data/inside/GenArk/pushRR/${machName}.today.quickLiftList.gz
  scpAtomic /dev/shm/${liftOverStat}.gz \
    /hive/data/inside/GenArk/pushRR/${machName}.today.liftOverList.gz
fi

rm -f /dev/shm/${fileList}.gz /dev/shm/${statList}.gz \
   /dev/shm/${quickLiftStat}.gz /dev/shm/${quickLiftFileList}.gz \
   /dev/shm/${liftOverStat}.gz /dev/shm/${liftOverFileList}.gz

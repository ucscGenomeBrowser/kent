#!/bin/bash

export TOP="/hive/data/inside/GenArk/pushRR"

cd "${TOP}"

export doNotCount="public.hub.txt|beta.hub.txt|alpha.hub.txt|user.hub.txt|hub.txt|/contrib/"

export devCount=`zegrep -c -v "${doNotCount}" dev.todayList.gz`
export betaCount=`zegrep -c -v "${doNotCount}" hgwbeta.todayList.gz`
export hgw1Count=`zegrep -c -v "${doNotCount}" hgw1.todayList.gz`

printf "### excluding the files:\n\t${doNotCount}\nin these counts.\n"

printf "### count of files on hgwdev: ${devCount}, beta: ${betaCount} and hgw1: ${hgw1Count}\n"


printf "### count of common files between beta and hgwdev,\nnot counting /contrib/ or the hub.txt files:\n"
zegrep -v "${doNotCount}" dev.todayList.gz | cut -f2 | sort \
  | join -t$'\t' - <(zegrep -v "${doNotCount}" hgwbeta.todayList.gz | cut -f2 | sort) | wc -l

printf "### count of files unique to beta compared to hgwdev,\nnot counting /contrib/ or the hub.txt files:\n"
zegrep -v "${doNotCount}" dev.todayList.gz | cut -f2 | sort \
  | join -v2 -t$'\t' - <(zegrep -v "${doNotCount}" hgwbeta.todayList.gz | cut -f2 | sort) | wc -l
zegrep -v "${doNotCount}" dev.todayList.gz | cut -f2 | sort \
  | join -v2 -t$'\t' - <(zegrep -v "${doNotCount}" hgwbeta.todayList.gz | cut -f2 | sort) > uniq.beta.not.hgwdev.list
zegrep -v "${doNotCount}" dev.todayList.gz | cut -f2 | sort \
  | join -v1 -t$'\t' - <(zegrep -v "${doNotCount}" hgwbeta.todayList.gz | cut -f2 | sort) > uniq.hgwdev.not.beta.list

printf "### count of common files between hgw1 and hgwbeta,\nnot counting /contrib/ or the hub.txt files:\n"

zegrep -v "${doNotCount}" hgwbeta.todayList.gz | cut -f2 | sort \
  | join -t$'\t' - <(zegrep -v "${doNotCount}" hgw1.todayList.gz | cut -f2 | sort) | wc -l

# what new needs to get from hgwdev go beta
rm -f new.files.ready.to.beta.txt
touch new.files.ready.to.beta.txt
if [ "${devCount}" -gt "${betaCount}" ]; then
   export newFiles=`echo ${devCount} ${betaCount} | awk '{printf "%d", $1-$2}'`
   printf "### ${newFiles} new files to go out from hgwdev to beta not /contrib/\n"
   zegrep -v "${doNotCount}" dev.todayList.gz | cut -f2 | sort \
     | join -v1 -t$'\t' - <(zegrep -v "${doNotCount}" hgwbeta.todayList.gz | cut -f2 | sort) | sort -u > new.files.ready.to.beta.txt
   head -3 new.files.ready.to.beta.txt
   printf " . . .\n"
   tail -3 new.files.ready.to.beta.txt
fi

printf "### files with different time stamps hgwdev to hgwbeta:\n"

rm -f new.beta.timeStamps.txt
zegrep -v "${doNotCount}" dev.todayList.gz | sort -k2 \
  | join -t$'\t' -1 2 -2 2 - <(zegrep -v "${doNotCount}" hgwbeta.todayList.gz | sort -k2) | awk -F$'\t' '$2 != $3' | cut -f1 | sort -u > new.beta.timeStamps.txt

if [ -s "new.beta.timeStamps.txt" ]; then
  head -3 new.beta.timeStamps.txt
  printf " . . .\n"
  tail -3 new.beta.timeStamps.txt
fi

# accumulate list for cluster-admin cron job rsync
#   from hgwbeta out to RR machines
rm -f rsync.gbdb.toRR.fileList.txt
rm -f rsync.gbdb.genark.fileList.txt

rm -f new.files.ready.to.go.txt
touch new.files.ready.to.go.txt
if [ "${betaCount}" -gt "${hgw1Count}" ]; then
   export newFiles=`echo ${betaCount} ${hgw1Count} | awk '{printf "%d", $1-$2}'`
   printf "### ${newFiles} new files to go out from beta to the RR not /contrib/\n"
   zegrep -v "${doNotCount}" hgwbeta.todayList.gz | cut -f2 | sort \
     | join -v1 -t$'\t' - <(zegrep -v "${doNotCount}" hgw1.todayList.gz | cut -f2 | sort) | sort -u > new.files.ready.to.go.txt
   head -3 new.files.ready.to.go.txt
   printf " . . .\n"
   tail -3 new.files.ready.to.go.txt
   touch rsync.gbdb.genark.fileList.txt rsync.gbdb.toRR.fileList.txt
   cat new.files.ready.to.go.txt >> rsync.gbdb.genark.fileList.txt
   sed -e 's#^#genark/#;'  new.files.ready.to.go.txt >> rsync.gbdb.toRR.fileList.txt
fi

printf "### files with different time stamps:\n"

rm -f new.timeStamps.txt
zegrep -v "${doNotCount}" hgwbeta.todayList.gz | sort -k2 \
  | join -t$'\t' -1 2 -2 2 - <(zegrep -v "${doNotCount}" hgw1.todayList.gz | sort -k2) | awk -F$'\t' '$2 != $3' | cut -f1 | sort -u > new.timeStamps.txt

if [ -s "new.timeStamps.txt" ]; then
  head new.timeStamps.txt
  touch rsync.gbdb.genark.fileList.txt rsync.gbdb.toRR.fileList.txt
  cat new.timeStamps.txt >> rsync.gbdb.genark.fileList.txt
  sed -e 's#^#genark/#;'  new.timeStamps.txt >> rsync.gbdb.toRR.fileList.txt
fi

./quickLiftNew.sh
./liftOverNew.sh

exit $?

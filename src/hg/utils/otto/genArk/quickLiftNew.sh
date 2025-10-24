#!/bin/bash

export TOP="/hive/data/inside/GenArk/pushRR"

cd "${TOP}"

export doNotCount="public.hub.txt|beta.hub.txt|alpha.hub.txt|user.hub.txt|hub.txt|/contrib/"

export devCount=`zegrep -c -v "${doNotCount}" dev.today.quickLiftList.gz`
export hgw1Count=`zegrep -c -v "${doNotCount}" hgw1.today.quickLiftList.gz`

printf "### excluding the files:\n\t"${doNotCount}"\nin these counts."

printf "### count of files on hgwdev: ${devCount} and hgw1: ${hgw1Count}\n"

printf "### count of common files between hgw1 and hgwdev,\nnot counting /contrib/ or the hub.txt files:\n"

zegrep -v "${doNotCount}" dev.today.quickLiftList.gz | cut -f2 | sort \
  | join -t$'\t' - <(zegrep -v "${doNotCount}" hgw1.today.quickLiftList.gz | cut -f2 | sort) | wc -l

# accumulate list for cluster-admin cron job rsync
#   from hgwbeta out to RR machines
rm -f rsync.gbdb.quickLift.fileList.txt

rm -f new.quickLift.ready.to.go.txt
touch new.quickLift.ready.to.go.txt
if [ "${devCount}" -gt "${hgw1Count}" ]; then
   export newFiles=`echo ${devCount} ${hgw1Count} | awk '{printf "%d", $1-$2}'`
   printf "### ${newFiles} new files to go out from hgwdev not /contrib/\n"
   zegrep -v "${doNotCount}" dev.today.quickLiftList.gz | cut -f2 | sort \
     | join -v1 -t$'\t' - <(zegrep -v "${doNotCount}" hgw1.today.quickLiftList.gz | cut -f2 | sort) | sort -u > new.quickLift.ready.to.go.txt
   head -3 new.quickLift.ready.to.go.txt
   printf " . . .\n"
   tail -3 new.quickLift.ready.to.go.txt
   touch rsync.gbdb.quickLift.fileList.txt
   cat new.quickLift.ready.to.go.txt >>  rsync.gbdb.quickLift.fileList.txt
fi

printf "### files with different time stamps:\n"

rm -f new.quickLift.timeStamps.txt
zegrep -v "${doNotCount}" dev.today.quickLiftList.gz | sort -k2 \
  | join -t$'\t' -1 2 -2 2 - <(zegrep -v "${doNotCount}" hgw1.today.quickLiftList.gz | sort -k2) | awk -F$'\t' '$2 != $3' | cut -f1 | sort -u > new.quickLift.timeStamps.txt

if [ -s "new.quickLift.timeStamps.txt" ]; then
   head new.quickLift.timeStamps.txt
   touch rsync.gbdb.quickLift.fileList.txt
   cat new.quickLift.timeStamps.txt >>  rsync.gbdb.quickLift.fileList.txt
fi

exit $?

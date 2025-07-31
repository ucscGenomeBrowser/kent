#!/bin/bash

#############################################################################
###  This source is from the source tree:
###     ~/kent/src/hg/utils/otto/genArk/pushRR.sh
###  do *not* edit this in the otto directory /hive/data/inside/GenArk/pushRR/
###  where this is used.
###
###  running in otto user crontab:
###  03 01 * * * /hive/data/inside/GenArk/pushRR/pushRR.sh
###
###  Expects to have listings made before this from other cron jobs:
###  -rw-r--r-- 1 5727305 Jul 30 10:33 hgwbeta.todayList.gz
###  -rw-r--r-- 1 6237612 Jul 30 11:03 hgw1.todayList.gz
###  -rw-r--r-- 1 6562130 Jul 30 22:28 dev.todayList.gz
###
#############################################################################

# exit on any error
set -beEu -o pipefail

export TOP="/hive/data/inside/GenArk/pushRR"
cd "${TOP}"
export msgTo="hclawson@ucsc.edu,otto-group@ucsc.edu"
export msgFile="/tmp/genarkPushRR.$$.txt"

if [ -s "lockFile.txt" ]; then
  printf "To: %s\n" "${msgTo}" > "${msgFile}"
  printf "From: hiram@soe.ucsc.edu\n" >> "${msgFile}"
  printf "Subject: ALERT: otto genArk pushRR overrun\n" >> "${msgFile}"
  printf "\n" >> "${msgFile}"
  printf "# warning: the /hive/data/inside/GenArk/pushRR/pushRR.sh script\n" >> "${msgFile}"
  printf "# is overrunning itself from yesterday, lockfile exists:\n" >> "${msgFile}"
  printf "# cat %s\n" "`pwd`/lockFile.txt" >> "${msgFile}"
  cat lockFile.txt >> "${msgFile}"
  cat "${msgFile}" | /usr/sbin/sendmail -t -oi
  rm -fr "${msgFile}"
  exit $?
fi

rm -f "${msgFile}"

date > lockFile.txt

export expectHost="hgwdev"

export hostName=$(hostname -s)
if [[ "${hostName}" != "${expectHost}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectHost}" "${hostName}" 1>&2
  exit 255
fi

export DS=`date "+%F"`
export TS=`date "+%T"`
export Y=`date "+%Y"`
export M=`date "+%m"`
export logDir="${TOP}/logs/${Y}/${M}"

mkdir -p "${logDir}"

logFile="${logDir}/rsync.pushRR.${DS}"

time (${TOP}/pushNewOnes.sh) >> "${logFile}" 2>&1

DS=`date "+%F"`
TS=`date "+%T"`
printf "### running quickPush.pl %s %s \n" "${DS}" "${TS}" >> "${logFile}"
printf "### the log for quickPush.pl will be in %s/quickBetaPublic...\n" "${logDir}" >> "${logFile}"
time (${TOP}/quickPush.pl) >> "${logFile}" 2>&1
DS=`date "+%F"`
TS=`date "+%T"`
printf "### finished quickPush.pl %s %s \n" "${DS}" "${TS}" >> "${logFile}"
gzip "${logFile}"

rm lockFile.txt

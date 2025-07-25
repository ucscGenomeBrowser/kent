#!/bin/bash

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

for M in hgwbeta hgw0 hgw1 hgw2 "Genome-Browser-Mirror-3.dhcp.uni-bielefeld.de"
do
  DS=`date "+%F"`
  TS=`date "+%T"`
  printf "### sending GCA %s %s to machine %s\n" "${DS}" "${TS}" "${M}" >> "${logFile}"
  time (rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" --exclude="contrib/" "/gbdb/genark/GCA/" "qateam@${M}:/gbdb/genark/GCA/") >> "${logFile}" 2>&1
  DS=`date "+%F"`
  TS=`date "+%T"`
  printf "### sending GCF %s %s to machine %s\n" "${DS}" "${TS}" "${M}" >> "${logFile}"
  time (rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" --exclude="contrib/" "/gbdb/genark/GCF/" "qateam@${M}:/gbdb/genark/GCF/") >> "${logFile}" 2>&1
done

DS=`date "+%F"`
TS=`date "+%T"`
printf "### running alphaBetaPush %s %s \n" "${DS}" "${TS}" >> "${logFile}"
time (./alphaBetaPush.pl)  >> "${logFile}" 2>&1
DS=`date "+%F"`
TS=`date "+%T"`
printf "### finished alphaBetaPush %s %s \n" "${DS}" "${TS}" >> "${logFile}"
gzip "${logFile}"

rm lockFile.txt

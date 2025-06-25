#!/bin/bash

# exit on any error
set -beEu -o pipefail

export TOP="/hive/data/inside/GenArk/pushRR"
cd "${TOP}"

export expectHost="hgwdev"

export hostName=$(hostname -s)
if [[ "${hostName}" != "${expectHost}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectHost}" "${hostName}" 1>&2
  exit 255
fi

export DS=`date "+%F"`
export Y=`date "+%Y"`
export M=`date "+%m"`
export logDir="${TOP}/logs/${Y}/${M}"

mkdir -p "${logDir}"

logFile="${logDir}/rsync.${DS}"

for M in hgwbeta hgw0 hgw1 hgw2 "Genome-Browser-Mirror-3.dhcp.uni-bielefeld.de"
do
  printf "### sending to machine %s\n" "${M}" >> "${logFile}"
  time (rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" --exclude="contrib/" "/gbdb/genark/GCA/" "qateam@${M}:/gbdb/genark/GCA/") >> "${logFile}" 2>&1
  time (rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" --exclude="contrib/" "/gbdb/genark/GCF/" "qateam@${M}:/gbdb/genark/GCF/") >> "${logFile}" 2>&1
done

printf "### running alphaBetaPush \n" >> "${logFile}"
time (./alphaBetaPush.pl)  >> "${logFile}" 2>&1
gzip "${logFile}"

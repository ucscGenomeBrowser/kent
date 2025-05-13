#!/bin/bash

# exit on any error
set -beEu -o pipefail

export expectName="hgwdev"

export uName=$(hostname -s)
if [[ "${uName}" != "${expectName}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectName}" "${uName}" 1>&2
  exit 255
fi

export DS=`date "+%F"`
export Y=`date "+%Y"`
export M=`date "+%m"`
export logDir="/hive/data/inside/genArk/logs/${Y}/${M}"

mkdir -p "${logDir}"


for M in hgwbeta hgw0 hgw1 hgw2 "Genome-Browser-Mirror-3.dhcp.uni-bielefeld.de"
do
  logFile="${logDir}/rsync.GCA.${M}.${DS}.gz"
  rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" "/gbdb/genark/GCA/" "qateam@${M}:/gbdb/genark/GCA/" 2>&1 | gzip -c > "${logFile}" 2>&1
  logFile="${logDir}/rsync.GCF.${M}.${DS}.gz"
  rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" "/gbdb/genark/GCF/" "qateam@${M}:/gbdb/genark/GCF/" 2>&1 | gzip -c > "${logFile}" 2>&1
done

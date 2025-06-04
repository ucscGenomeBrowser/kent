#!/bin/bash

# exit on any error
set -beEu -o pipefail

export expectName="genomebrowsermirror3"

export uName=$(hostname -s)
if [[ "${uName}" != "${expectName}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectName}" "${uName}" 1>&2
  exit 255
fi

export DS=`date "+%F"`
export Y=`date "+%Y"`
export M=`date "+%m"`
export logDir="/home/qateam/rsyncLog/${Y}/${M}"

mkdir -p "${logDir}"
export srcMachine="hgwdev.gi.ucsc.edu"

logFile="${logDir}/rsync.GCA.${M}.${DS}.gz"
time (rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" "qateam@${srcMachine}:/gbdb/genark/GCA/" "/gbdb/genark/GCA/") | gzip -c > "${logFile}" 2>&1
logFile="${logDir}/rsync.GCF.${M}.${DS}.gz"
time (rsync --delete --stats -a -L --itemize-changes --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" "qateam@${srcMachine}:/gbdb/genark/GCF/" "/gbdb/genark/GCF/") | gzip -c > "${logFile}" 2>&1

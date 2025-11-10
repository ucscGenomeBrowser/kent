#!/bin/bash

set -beEu -o pipefail

export machList="hgw0 hgw1 hgw2 genome-browser-mirror-3.dhcp.uni-bielefeld.de"

export TOP="/hive/data/inside/GenArk/pushRR"
export filesFrom="${TOP}/rsync.gbdb.toRR.fileList.txt"
cd "${TOP}"

function usage() {
  printf "usage: /hive/data/inside/GenArk/pushRR/pushFromBetaToRR.sh makeItSo\n" 1>&2
  printf "\nOnly functions if used by root user.\n" 1>&2
  printf "This script will push out from hgwbeta new or updated files only from:\n" 1>&2
  printf "\t\t/gbdb/*/quickLift/ and /gbdb/genark/\n" 1>&2
  printf "to the machines: %s\n" "${machList}" 1>&2
  printf "Controlled by the list of files in the file list created by otto cron jobs:\n" 1>&2
  printf "\t${filesFrom}\n" 1>&2
}
if [ $# -ne 1 ]; then
  usage
  exit 255
fi

export makeItSo="${1}"
if [ "${makeItSo}" != "makeItSo" ]; then
  printf "ERROR: the argument *must* be: 'makeItSo'\n" 1>&2
  usage
  exit 255
fi

# this EUID bash internal variable is supposed to exist
#   in any bash shell version >= 2.  Does not matter how the script is run.
if [ "$EUID" -ne 0 ]; then
  echo "ERROR: This script must be run as root." 1>&2
  usage
  exit 255
fi

### only need to run if this file has content:
if [ -s "$filesFrom" ]; then
   for M in $machList
   do
     printf "time (rsync --stats -a -L --files-from=$filesFrom \"/gbdb/\" \"qateam@${M}:/gbdb/\") 2>&1\n" 1>&2
     time (rsync --stats -a -L --files-from=$filesFrom "/gbdb/" "qateam@${M}:/gbdb/") 2>&1
   done
fi

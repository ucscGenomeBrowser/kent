#!/bin/bash

# used on the hgdownload machine in the directory:
#	/home/qateam/cronJobs/hubTxtAudit.sh
# weekly audit for asmHubList.pl, which only looks at hubs named in the
#  clade listings.  This does the full slow 'find' through the
#  /mirrordata/hubs/GC[AF] hierarchy and reports:
#   1. hub directories without a hub.txt file
#   2. hub directories with a hub.txt that are not in any clade listing,
#      these do not appear in UCSC_GI.assemblyHubList.txt
# no output from this script means everything is accounted for

set -beEu -o pipefail

export thisMachine=`hostname -s`

if [ "$thisMachine" != "hgdownload1" -a "$thisMachine" != "hgdownload2" -a "$thisMachine" != "hgdownload3" ]; then
  printf "# NOTE: This script is only used on hgdownload[123] '$thisMachine'\n" 1>&2
  exit 255
fi

cd /home/qateam/cronJobs

export tmpDir=`mktemp -d /dev/shm/hubTxtAudit.XXXXXX`
trap "rm -fr ${tmpDir}" EXIT

./cladeListings.sh > /dev/null

# every accession the clade listings know about, same as asmHubList.pl uses
(cat /dev/shm/clade.*.list; cut -f1 legacy.clade.txt | cut -d'_' -f1-2) \
  | sort -u > "${tmpDir}/listed.txt"

find /mirrordata/hubs/GCF /mirrordata/hubs/GCA -mindepth 4 -maxdepth 4 \
  -type d | sort > "${tmpDir}/dirs.txt"

while read hubDir
do
  if [ -s "${hubDir}/hub.txt" ]; then
    printf "%s\n" "${hubDir##*/}" >> "${tmpDir}/haveHubTxt.txt"
  else
    printf "%s\n" "${hubDir}" >> "${tmpDir}/noHubTxt.txt"
  fi
done < "${tmpDir}/dirs.txt"

touch "${tmpDir}/haveHubTxt.txt" "${tmpDir}/noHubTxt.txt"

sort -u "${tmpDir}/haveHubTxt.txt" > "${tmpDir}/haveHubTxt.sorted.txt"
comm -23 "${tmpDir}/haveHubTxt.sorted.txt" "${tmpDir}/listed.txt" \
  > "${tmpDir}/notListed.txt"

export noHubTxt=$(grep -c . "${tmpDir}/noHubTxt.txt")
export notListed=$(grep -c . "${tmpDir}/notListed.txt")

if [ "${noHubTxt}" -gt 0 ]; then
  printf "# %d hub directories without a hub.txt:\n" "${noHubTxt}"
  cat "${tmpDir}/noHubTxt.txt"
fi

if [ "${notListed}" -gt 0 ]; then
  printf "# %d hubs with a hub.txt not in any clade listing:\n" "${notListed}"
  cat "${tmpDir}/notListed.txt"
fi

#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: ./sendToHgdownload.sh <GCF/012/345/678/GCF_012345678.nn>\n" 1>&2
  exit 255
fi

export dirPath="${1}"
export accession=`basename ${dirPath}`
# hgdownload1 returns to service 02 April 2024, push to both machines
export downloadDest1="hgdownload1.soe.ucsc.edu"
export downloadDest2="hgdownload2.soe.ucsc.edu"
export downloadDest3="hgdownload3.gi.ucsc.edu"
# 2025-03-05 hgdownload3.gi.ucsc.edu has address 169.233.10.12
# 2024-02-06 hgdownload2.gi.ucsc.edu has address 128.114.198.53
# 2024-04-03
# host hgdownload1.gi.ucsc.edu
# Host hgdownload1.gi.ucsc.edu not found: 3(NXDOMAIN)
# host hgdownload1.soe.ucsc.edu
# hgdownload1.soe.ucsc.edu has address 128.114.119.163

# host hgdownload2.gi.ucsc.edu
# hgdownload2.gi.ucsc.edu has address 128.114.198.53
# host hgdownload2.soe.ucsc.edu
# hgdownload2.soe.ucsc.edu is an alias for hgdownload2.gi.ucsc.edu.
# hgdownload2.gi.ucsc.edu has address 128.114.198.53

export tB="/hive/data/genomes/asmHubs/allBuild/${dirPath}"
export dCount=`ls -d ${tB}_* | wc -l`
if [ "${dCount}" -ne 1 ]; then
   printf "ERROR: can not find build directory\n%s_*\n" "${tB}"
   exit 255
fi
export tD=`ls -d ${tB}_*`
export buildDir=`realpath ${tD}`
if [ ! -d "${buildDir}" ]; then
   printf "ERROR: can not find build directory\n%s\n" "${td}"
   exit 255
fi
## verify no broken symlinks
export srcDir="/hive/data/genomes/asmHubs/${dirPath}"
if [ ! -d "${srcDir}" ]; then
   printf "# WARNING: no srcDir: '%s'\n" "${srcDir}" 1>&2
   exit 0
fi

badLinks=`(find "${srcDir}" -type l -lname \* \
  | xargs --no-run-if-empty ls -lL > /dev/null || true) 2>&1 | wc -l`

### printf "# badLinks: %s\n" "${badLinks}"

if [ "${badLinks}" -gt 0 ]; then
  printf "ERROR: missing symlink targets:\n" 1>&2
  find "/hive/data/genomes/asmHubs/${dirPath}" -type l -lname \* \
    | xargs --no-run-if-empty ls -lL > /dev/null
  exit 255
fi

export DS=`date "+%F %T"`
export destDir="/mirrordata/hubs/${dirPath}"
printf "### sending %s\t%s\t##########\n" "`basename ${srcDir}`" "${DS}"
printf "# srcDir: %s\n" "${srcDir}"
printf "# destDir: %s\n" "${destDir}"

ssh qateam@${downloadDest1} "mkdir -p ${destDir}" 2>&1 | grep -v "X11 forwarding request" || true &
# ssh qateam@${downloadDest2} "mkdir -p ${destDir}" 2>&1 | grep -v "X11 forwarding request" || true
ssh qateam@${downloadDest3} "mkdir -p ${destDir}" 2>&1 | grep -v "X11 forwarding request" || true
wait
# printf "# successful mkdir on ${downloadDest1}, ${downloadDest2} and ${downloadDest3}\n"
printf "# successful mkdir on ${downloadDest1} and ${downloadDest3}\n"

### 2021-12-20 - out of disk space on dynablat-01
### 2022-06-01 new dynamic-01 machine more disk space

export dynaBlat="dynablat-01.soe.ucsc.edu"

# export dynaBlat="128.114.119.136"

### check if there are actually index files to go:

export idxCount=`ls ${buildDir}/*.gfidx 2> /dev/null | wc -l`

if [ "${idxCount}" -eq 2 ]; then

export dynaServerDir="/scratch/hubs/${dirPath}"

ssh qateam@$dynaBlat "mkdir -p ${dynaServerDir}" 2>&1 | grep -v "X11 forwarding request" || true
printf "# successful mkdir on $dynaBlat\n" 1>&2

printf "rsync --stats -a -L -P --exclude=\"md5sum.txt\" ${srcDir}/${accession}.2bit \"qateam@$dynaBlat:${dynaServerDir}/\"\n" 1>&2
rsync --stats -a -L -P --exclude="md5sum.txt" ${srcDir}/${accession}.2bit "qateam@$dynaBlat:${dynaServerDir}/" \
  2>&1 | grep -v "X11 forwarding request"
printf "rsync --stats -a -L -P --exclude=\"md5sum.txt\" ${buildDir}/*.gfidx \"qateam@$dynaBlat:${dynaServerDir}/\"\n" 1>&2
rsync --stats -a -L -P --exclude="md5sum.txt" ${buildDir}/*.gfidx "qateam@$dynaBlat:${dynaServerDir}/" \
  2>&1 | grep -v "X11 forwarding request"

else
  printf "warning: no dynamic server indexes found in ${buildDir}/*.gfidx\n" 1>&2
fi

# the single file hub genome trackDb file:

printf "rsync --delete --exclude=\"md5sum.txt\"--exclude=\"alpha.hub.txt\" --exclude=\"beta.hub.txt\" --exclude=\"public.hub.txt\" --exclude=\"user.hub.txt\" --exclude=\"hub.txt\" --exclude=\"download.hub.txt\" --stats -a -L -P \"${srcDir}/\" \"qateam@${downloadDest1}:${destDir}/\"\n" 1>&2
# printf "rsync --delete --exclude=\"alpha.hub.txt\" --exclude=\"beta.hub.txt\"  --exclude=\"public.hub.txt\" --exclude=\"user.hub.txt\" --exclude=\"hub.txt\" --exclude=\"download.hub.txt\" --stats -a -L -P \"${srcDir}/\" \"qateam@${downloadDest2}:${destDir}/\"\n" 1>&2
printf "rsync --delete --exclude=\"md5sum.txt\" --exclude=\"alpha.hub.txt\" --exclude=\"beta.hub.txt\" --exclude=\"public.hub.txt\" --exclude=\"user.hub.txt\" --exclude=\"hub.txt\" --exclude=\"download.hub.txt\" --stats -a -L -P \"${srcDir}/\" \"qateam@${downloadDest3}:${destDir}/\"\n" 1>&2
rsync --delete --exclude="md5sum.txt" --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt"   --exclude="user.hub.txt" --exclude="hub.txt" --exclude="download.hub.txt" --stats -a -L -P "${srcDir}/" "qateam@${downloadDest1}:${destDir}/" \
  2>&1 | grep -v "X11 forwarding request" &
# rsync --delete --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" --exclude="hub.txt" --exclude="download.hub.txt" --stats -a -L -P "${srcDir}/" "qateam@${downloadDest2}:${destDir}/" \
#  2>&1 | grep -v "X11 forwarding request"
rsync --delete --exclude="md5sum.txt" --exclude="alpha.hub.txt" --exclude="beta.hub.txt" --exclude="public.hub.txt" --exclude="user.hub.txt" --exclude="hub.txt" --exclude="download.hub.txt" --stats -a -L -P "${srcDir}/" "qateam@${downloadDest3}:${destDir}/" \
  2>&1 | grep -v "X11 forwarding request"
wait

# the new single file hub genome trackDb file:
printf "rsync --stats -a -L -P \"${srcDir}/download.hub.txt\" \"qateam@${downloadDest1}:${destDir}/hub.txt\"\n" 1>&2
# printf "rsync --stats -a -L -P \"${srcDir}/download.hub.txt\" \"qateam@${downloadDest2}:${destDir}/hub.txt\"\n" 1>&2
printf "rsync --stats -a -L -P \"${srcDir}/download.hub.txt\" \"qateam@${downloadDest3}:${destDir}/hub.txt\"\n" 1>&2
rsync --stats -a -L -P "${srcDir}/download.hub.txt" "qateam@${downloadDest1}:${destDir}/hub.txt" \
  2>&1 | grep -v "X11 forwarding request" &
# rsync --stats -a -L -P "${srcDir}/download.hub.txt" "qateam@${downloadDest2}:${destDir}/hub.txt" \
#  2>&1 | grep -v "X11 forwarding request"
rsync --stats -a -L -P "${srcDir}/download.hub.txt" "qateam@${downloadDest3}:${destDir}/hub.txt" \
  2>&1 | grep -v "X11 forwarding request"
wait
printf "# successful rsync\n"

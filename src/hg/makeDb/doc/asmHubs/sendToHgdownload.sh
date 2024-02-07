#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: ./sendToHgdownload.sh <GCF/012/345/678/GCF_012345678.nn>\n" 1>&2
  exit 255
fi

export dirPath="${1}"
export accession=`basename ${dirPath}`

## verify no broken symlinks
export srcDir="/hive/data/genomes/asmHubs/${dirPath}"

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

ssh qateam@hgdownload.soe.ucsc.edu "mkdir -p ${destDir}" 2>&1 | grep -v "X11 forwarding request" || true
printf "# successful mkdir on hgdownload\n"

### 2021-12-20 - out of disk space on dynablat-01
### 2022-06-01 new dynamic-01 machine more disk space

export dynaBlat="dynablat-01.soe.ucsc.edu"

# export dynaBlat="128.114.119.136"

### check if there are actually index files to go:

export idxCount=`ls ${srcDir}/*.gfidx 2> /dev/null | wc -l`

if [ "${idxCount}" -gt 0 ]; then

export dynaServerDir="/scratch/hubs/${dirPath}"

ssh qateam@$dynaBlat "mkdir -p ${dynaServerDir}" 2>&1 | grep -v "X11 forwarding request" || true
printf "# successful mkdir on $dynaBlat\n" 1>&2

printf "rsync --stats -a -L -P ${srcDir}/${accession}.2bit \"qateam@$dynaBlat:${dynaServerDir}/\"\n" 1>&2
rsync --stats -a -L -P ${srcDir}/${accession}.2bit "qateam@$dynaBlat:${dynaServerDir}/" \
  2>&1 | grep -v "X11 forwarding request"
printf "rsync --stats -a -L -P ${srcDir}/*.gfidx \"qateam@$dynaBlat:${dynaServerDir}/\"\n" 1>&2
rsync --stats -a -L -P ${srcDir}/*.gfidx "qateam@$dynaBlat:${dynaServerDir}/" \
  2>&1 | grep -v "X11 forwarding request"

fi

# the new single file hub genome trackDb file:
# genomes.txt obsolete now with the single file
# ssh qateam@hgdownload.soe.ucsc.edu "rm ${destDir}/genomes.txt" 2>&1 | egrep -v "cannot remove|X11 forwarding request" || true
# ssh qateam@hgdownload.soe.ucsc.edu "rm ${destDir}/html/*.description.html" 2>&1 | grep -v "X11 forwarding request" || true
printf "rsync --delete --exclude=\"hub.txt\" --exclude=\"download.hub.txt\" --stats -a -L -P \"${srcDir}/\" \"qateam@hgdownload.soe.ucsc.edu:${destDir}/\"\n" 1>&2
rsync --delete --exclude="hub.txt" --exclude="download.hub.txt" --stats -a -L -P "${srcDir}/" "qateam@hgdownload.soe.ucsc.edu:${destDir}/" \
  2>&1 | grep -v "X11 forwarding request"
# the new single file hub genome trackDb file:
printf "rsync --stats -a -L -P \"${srcDir}/download.hub.txt\" \"qateam@hgdownload.soe.ucsc.edu:${destDir}/hub.txt\"\n" 1>&2
rsync --stats -a -L -P "${srcDir}/download.hub.txt" "qateam@hgdownload.soe.ucsc.edu:${destDir}/hub.txt" \
  2>&1 | grep -v "X11 forwarding request"
printf "# successful rsync\n"

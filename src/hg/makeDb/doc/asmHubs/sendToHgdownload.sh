#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: ./sendToHgdownload.sh <GCF/012/345/678/GCF_012345678.nn>\n" 1>&2
  exit 255
fi

export dirPath="${1}"

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

export destDir="/mirrordata/hubs/${dirPath}"
printf "# srcDir: %s\n" "${srcDir}"
printf "# destDir: %s\n" "${destDir}"

export dynaServerDir="/scratch/hubs/${dirPath}"

ssh qateam@hgdownload.soe.ucsc.edu "mkdir -p ${destDir}" 2>&1 | grep -v "X11 forwarding request" || true
printf "# successful mkdir on hgdownload\n"

ssh qateam@dynablat-01.soe.ucsc.edu "mkdir -p ${dynaServerDir}" 2>&1 | grep -v "X11 forwarding request" || true
printf "# successful mkdir on dynablat-01\n"

rsync --stats -a -L -P ${srcDir}/*.2bit "qateam@dynablat-01.soe.ucsc.edu:${dynaServerDir}/" \
  2>&1 | grep -v "X11 forwarding request" || true
rsync --stats -a -L -P ${srcDir}/*.gfidx "qateam@dynablat-01.soe.ucsc.edu:${dynaServerDir}/" \
  2>&1 | grep -v "X11 forwarding request" || true

# the new single file hub genome trackDb file:
# genomes.txt obsolete now with the single file
# ssh qateam@hgdownload.soe.ucsc.edu "rm ${destDir}/genomes.txt" 2>&1 | egrep -v "cannot remove|X11 forwarding request" || true
# ssh qateam@hgdownload.soe.ucsc.edu "rm ${destDir}/html/*.description.html" 2>&1 | grep -v "X11 forwarding request" || true
rsync --delete --exclude="hub.txt" --exclude="download.hub.txt" --stats -a -L -P "${srcDir}/" "qateam@hgdownload.soe.ucsc.edu:${destDir}/" \
  2>&1 | grep -v "X11 forwarding request" || true
# the new single file hub genome trackDb file:
rsync --stats -a -L -P "${srcDir}/download.hub.txt" "qateam@hgdownload.soe.ucsc.edu:${destDir}/hub.txt" \
  2>&1 | grep -v "X11 forwarding request" || true
printf "# successful rsync\n"

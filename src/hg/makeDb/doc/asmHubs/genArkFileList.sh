#!/bin/bash

# used on the hgdownload machine in the directory:
#	/home/qateam/cronJobs/genArkFileList.sh
# runs a find on the /mirrordata/hubs/GC[AF] directories to make
#      a GenArk file listing.

set -beEu -o pipefail

export thisMachine=`hostname -s`

if [ "$thisMachine" != "hgdownload" ]; then
  if [ "$thisMachine" != "hgdownload1" -a "$thisMachine" != "hgdownload2" -a "$thisMachine" != "hgdownload3" ]; then
      printf "# NOTE: This script is only used on hgdownload or hgdownload[123] '$thisMachine'\n" 1>&2
      exit 255
  fi
fi

cd /home/qateam/cronJobs

find /mirrordata/hubs/GCF /mirrordata/hubs/GCA -type f \
  | sed -e 's#/mirrordata/hubs/##;' | gzip -c \
     > /mirrordata/hubs/genArkFileList.txt.gz

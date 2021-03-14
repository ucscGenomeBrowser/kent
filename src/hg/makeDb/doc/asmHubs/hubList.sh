#!/bin/bash

# used on the hgdownload machine in the directory:
#	/home/qateam/cronJobs/hubList.sh
# scans the /mirrordata/hubs/GC[AF] hierarchy to look for populated
#  assembly hub directories.
# It collects the results into /mirrordata/hubs/UCSC_GI.assemblyHubList.txt
#  for use as a reference from the index.html page there as a convenient
#  text listing

set -beEu -o pipefail

export thisMachine=`uname -n`

if [ "$thisMachine" != "hgdownload" ]; then
  printf "# NOTE: This script is only used on hgdownload\n" 1>&2
  exit 255
fi

cd /home/qateam/cronJobs

export DS=`date "+%F"`

export before=`grep -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt | wc -l`

export asmCount=`find /mirrordata/hubs/GCF /mirrordata/hubs/GCA -mindepth 4 -maxdepth 4 -type d | wc -l`

printf "# This file: https://hgdownload.soe.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt
# last update: ${DS}, $asmCount assemblies listed
#
# Providing a listing of assembly resources available for display in
# the UCSC Genome Browser.  Tab separated list currently with four columns.
# Construct a short URL with the accession name to display this assembly
# in the genome browser. For example: https://genome.ucsc.edu/h/GCA_003369685.2
#
# additional columns may be added in the future (for example taxonId)
# (some assemblies have taxonId at this time, soon to have them all)
#
# accession\tassembly\tscientific name\tcommon name\ttaxonId
" > /mirrordata/hubs/UCSC_GI.assemblyHubList.txt

./asmHubList.pl goForIt | sort >> /mirrordata/hubs/UCSC_GI.assemblyHubList.txt

export after=`grep -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt | wc -l`

printf "# before count: $before, after count: $after\n" 1>&2


#!/bin/bash

# used on the hgdownload machine in the directory:
#	/home/qateam/cronJobs/hubList.sh
# scans the /mirrordata/hubs/GC[AF] hierarchy to look for populated
#  assembly hub directories.
# It collects the results into /mirrordata/hubs/UCSC_GI.assemblyHubList.txt
#  for use as a reference from the index.html page there as a convenient
#  text listing

set -beEu -o pipefail

export thisMachine=`hostname -s`

if [ "$thisMachine" != "hgdownload" ]; then
  if [ "$thisMachine" != "hgdownload1" -a "$thisMachine" != "hgdownload2" -a "$thisMachine" != "hgdownload3" ]; then
      printf "# NOTE: This script is only used on hgdownload or hgdownload[123] '$thisMachine'\n" 1>&2
      exit 255
  fi
fi

cd /home/qateam/cronJobs

rsync -a hiram@hgwdev:kent/src/hg/makeDb/doc/legacyAsmHub/legacy.clade.txt ./

export DS=`date "+%F"`

export before=`grep -c -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt`

export taxIdCount=`grep -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt | awk -F$'\t' '{print $5}' | sort -u | wc -l`

printf "taxIdCount before: $taxIdCount\n"

./cladeListings.sh 1>&2

export asmCount=`find /mirrordata/hubs/GCF /mirrordata/hubs/GCA -mindepth 4 -maxdepth 4 -type d | wc -l`

printf "# This file: https://hgdownload.soe.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt
# last update: ${DS}, $asmCount assemblies listed
#
# Providing a listing of assembly resources available for display in
# the UCSC Genome Browser.  Tab separated list currently with seven columns,
# additional columns may be added in the future.
# Construct a short URL with the accession name to display this assembly
# in the genome browser. For example: https://genome.ucsc.edu/h/GCA_003369685.2
#
# Note: a clade with suffix '(L)' is found in the 'legacy' set of assemblies.
# 'legacy' are assemblies that have become outdated and superseded by newer
# and better assemblies.
#
# currently clade groupings found in the GenArk are:
# birds fish fungi invertebrate mammals plants primates vertebrate viral bacteria
#
# where 'vertebrate' are the 'other' vertebrates not alreaded included in
#    birds fish mammals primates
#
# 'invertebrate' can include a broad range of organisms including single
#    cell eukaryotic protists etc.
#
# 'viral' currently includes most viruses of interest related to human
#         pathogens
#
# future plans to include selected sets of bacteria and archaea assemblies
#
# accession\tassembly\tscientific name\tcommon name\ttaxonId\tGenArk clade
" > /mirrordata/hubs/UCSC_GI.assemblyHubList.txt

./asmHubList.pl goForIt | grep -v _009914755 | sort >> /mirrordata/hubs/UCSC_GI.assemblyHubList.txt

export after=`grep -c -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt`

printf "# before count: $before, after count: $after\n" 1>&2

taxIdCount=`grep -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt | awk -F$'\t' '{print $5}' | sort -u | wc -l`
printf "taxIdCount after: $taxIdCount\n"

printf "# summary of *all* clades in UCSC_GI.assemblyHubList.txt\n"

grep -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt \
  | awk -F$'\t' '{print $NF}' | sed -e 's/(L)//;' | sort | uniq -c

printf "# summary of only the legacy clades in UCSC_GI.assemblyHubList.txt\n"

grep -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt \
  | grep "(L)" | awk -F$'\t' '{print $NF}' | sed -e 's/(L)//;' | sort | uniq -c

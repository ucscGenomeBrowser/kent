#!/bin/bash

# check for existence of trackData/<trackName> directory in the build
# directory of each assembly listed in column 1 of an orderList.tsv file

set -beEu -o pipefail

if [ $# -ne 2 ]; then
  printf "usage: trackDataExists.sh <orderList.tsv> <trackName>\n" 1>&2
  printf "e.g.:  trackDataExists.sh brc.orderList.tsv ncbiGene\n" 1>&2
  printf "output to stdout: exists|missing<tab>asmId<tab>directory\n" 1>&2
  printf "summary counts to stderr\n" 1>&2
  exit 255
fi

export orderList="$1"
export trackName="$2"

if [ ! -s "${orderList}" ]; then
  printf "ERROR: can not find orderList file: '%s'\n" "${orderList}" 1>&2
  exit 255
fi

export topDir="/hive/data/genomes/asmHubs"
export existCount=0
export missCount=0
export badCount=0

while IFS=$'\t' read -r asmId rest; do
  case "${asmId}" in
    ""|\#*) continue ;;
  esac
  case "${asmId}" in
    GCA_[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]*) buildDir="genbankBuild" ;;
    GCF_[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]*) buildDir="refseqBuild" ;;
    *)
      printf "WARNING: not a GCA/GCF name: '%s'\n" "${asmId}" 1>&2
      badCount=$((badCount + 1))
      continue
      ;;
  esac
  gcX="${asmId:0:3}"
  d0="${asmId:4:3}"
  d1="${asmId:7:3}"
  d2="${asmId:10:3}"
  trackDir="${topDir}/${buildDir}/${gcX}/${d0}/${d1}/${d2}/${asmId}/trackData/${trackName}"
  if [ -d "${trackDir}" ]; then
    printf "exists\t%s\t%s\n" "${asmId}" "${trackDir}"
    existCount=$((existCount + 1))
  else
    printf "missing\t%s\t%s\n" "${asmId}" "${trackDir}"
    missCount=$((missCount + 1))
  fi
done < "${orderList}"

printf "# %s: %d exists, %d missing, %d bad names\n" "${trackName}" \
  "${existCount}" "${missCount}" "${badCount}" 1>&2

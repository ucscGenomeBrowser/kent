#!/bin/bash

set -beEu -o pipefail

if [ $# -lt 1 ]; then
  printf "usage: gdDbLinks.sh asmId [... asmId ...]\n" 1>&2
  exit 255
fi

for asmId in $*
do
  gcX="${asmId:0:3}"
  d0="${asmId:4:3}"
  d1="${asmId:7:3}"
  d2="${asmId:10:3}"
  acc=`echo $asmId | cut -d'_' -f1-2`
  downDir="/hive/data/genomes/asmHubs/${gcX}/${d0}/${d1}/${d2}/${acc}"
  dbDbDir="/gbdb/genark/${gcX}/${d0}/${d1}/${d2}/${acc}"
  if [ ! -d "${dbDbDir}" ]; then
    mkdir -p "${dbDbDir}"
    mkdir -p "${dbDbDir}/html"
    mkdir -p "${dbDbDir}/bbi"
    mkdir -p "${dbDbDir}/ixIxx"
  fi
  for ext in 2bit 2bit.bpt chrom.sizes.txt chromAlias.bb
  do
    rm -f "${dbDbDir}/${acc}.${ext}"
    ln -s "${downDir}/${acc}.${ext}" "${dbDbDir}/${acc}.${ext}"
  done
  for txt in hub groups
  do
    rm -f "${dbDbDir}/${txt}.txt"
    ln -s "${downDir}/${txt}.txt" "${dbDbDir}/${txt}.txt"
  done
  # clear entirely the html directory
  (ls ${dbDbDir}/html/*.html 2> /dev/null || true) | while read htmlFile
  do
     rm -f "${htmlFile}"
  done
  for htmlFile in ${downDir}/html/*.html
  do
    fileName=`basename $htmlFile`
    ln -s "${htmlFile}" "${dbDbDir}/html/${fileName}"
  done
  # clear entirely the bbi directory
  (ls ${dbDbDir}/bbi/*.bb ${dbDbDir}/bbi/*.bw 2> /dev/null || true) | while read bigFile
  do
     rm -f "${bigFile}"
  done
  for bigFile in ${downDir}/bbi/*.bb ${downDir}/bbi/*.bw
  do
    fileName=`basename $bigFile`
    ln -s "${bigFile}" "${dbDbDir}/bbi/${fileName}"
  done
done

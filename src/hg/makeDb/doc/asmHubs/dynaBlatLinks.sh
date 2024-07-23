#!/bin/bash

set -beEu -o pipefail

if [ $# -lt 1 ]; then
  printf "usage: gdDbLinks.sh asmId [... asmId ...]\n" 1>&2
  exit 255
fi

export dynaBlatLinks="/hive/data/inside/dynaBlat/backUpLinks"

for asmId in $*
do
  gcX="${asmId:0:3}"
  d0="${asmId:4:3}"
  d1="${asmId:7:3}"
  d2="${asmId:10:3}"
  acc=`echo $asmId | cut -d'_' -f1-2`
  downDir="/hive/data/genomes/asmHubs/${gcX}/${d0}/${d1}/${d2}/${acc}"
  linksDir="${dynaBlatLinks}/${gcX}/${d0}/${d1}/${d2}/${acc}"
  if [ ! -d "${linksDir}" ]; then
    mkdir -p "${linksDir}"
  fi
  for ext in 2bit trans.gfidx untrans.gfidx
  do
    rm -f "${linksDir}/${acc}.${ext}"
    ln -s "${downDir}/${acc}.${ext}" "${linksDir}/${acc}.${ext}"
  done
done

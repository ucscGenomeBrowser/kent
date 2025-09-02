#!/bin/bash

set -beEu -o pipefail

if [ $# -lt 1 ]; then
  printf "usage: gdDbLinks.sh asmId [... asmId ...]\n" 1>&2
  printf "or with the single argument: 'ucscDb' to check the database links\n" 1>&2
  exit 255
fi

export dynaBlatLinks="/hive/data/inside/dynaBlat/backUpLinks"

if [ $# -eq 1 ]; then
  asmId=$*
  if [ "${asmId}" = "ucscDb" ]; then
    for D in /hive/data/genomes/*/dynamicBlat
    do
       dbDir=`dirname "${D}"`
       db=`basename "${dbDir}"`
       twoBit="${dbDir}/${db}.2bit"
       if [ -s "${twoBit}" ]; then
         printf "2bit: '%s'\n" "${twoBit}"
         mkdir -p "${dynaBlatLinks}/${db}"
         ln -s "${twoBit}" "${dynaBlatLinks}/${db}"
         for ext in untrans.gfidx trans.gfidx
         do
           filePath="${D}/${db}.${ext}"
           if [ ! -s "${filePath}" ]; then
              printf "missing: '%s'\n" "${filePath}"
              exit 255
           else
              ln -s "${filePath}" "${dynaBlatLinks}/${db}"
           fi
         done
       else
         printf "ERROR: can not find 2bit file: '%s'\n" "${twoBit}" 1>&2
       fi
    done
  else
    printf "# arge 1 '%s'\n" "$*"
  fi
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

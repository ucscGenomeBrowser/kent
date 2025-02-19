#!/bin/bash

set -beEu -o pipefail

set -x

if [ $# -lt 1 ]; then
  printf "usage: reRunTrackDb.sh asmId [... asmId ...]\n" 1>&2
  printf "the asmId argument can instead be a file with a list of asmIds\n" 1>&2
  exit 255
fi

function runOne() {
  export asmId="${1}"
  gcX="${asmId:0:3}"
  d0="${asmId:4:3}"
  d1="${asmId:7:3}"
  d2="${asmId:10:3}"
  tDir="/hive/data/genomes/asmHubs/allBuild/${gcX}/${d0}/${d1}/${d2}/${asmId}"
  buildDir=`realpath ${tDir}`
  if [ -d "${buildDir}" ]; then
    doTdb="${buildDir}/doTrackDb.bash"
    tdb="${buildDir}/${asmId}.trackDb.txt"
    if [ -s "${tdb}" ]; then
      if [ -s "${doTdb}" ]; then
        printf "%s\t%s\n" "${asmId}" "`stat --printf=\"%Y\t%y\t%n\n\" \"${tdb}\"`" 1>&2
        printf "%s\t%s\n" "${asmId}" "`stat --printf=\"%Y\t%y\t%n\n\" \"${doTdb}\"`" 1>&2
        bCount=`grep -c -w track ${tdb}`
        ${doTdb} > /dev/null
        aCount=`grep -c -w track ${tdb}`
        printf "%s\t%d\t%d\n" "${asmId}" "${bCount}" "${aCount}"
      else
        printf "ERROR: missing doTdb\n" "${doTdb}" 1>&2
      fi
    else
      printf "ERROR: can not find trackDb.txt\n%s\n" "${tdb}" 1>&2
    fi
  else
    printf "ERROR: can not find buildDir\n%s\n" "${tDir}" 1>&2
  fi
}

for asmIdFile in $*
do
  export asmId="${asmIdFile}"
  if [ -s "${asmIdFile}" ]; then
     for asmId in `cut -f1 ${asmIdFile}`
     do
        runOne "${asmId}"
     done
  else
        runOne "${asmId}"
  fi
done

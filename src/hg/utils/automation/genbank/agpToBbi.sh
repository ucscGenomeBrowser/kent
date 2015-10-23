#!/bin/bash

set -beEu -o pipefail

if [ $# -lt 5 ]; then
  printf "usage: agpToBbi.sh [ucsc|ncbi] <dbPrefix> <db.chrom.sizes> <pathTo/directoryWith/*.agp.gz> <pathTo>/bbi\n" 1>&2
  printf "constructs dbPrefix.assembly.bb and dbPrefix.gap.bb in <pathTo>/bbi/\n" 1>&2
  printf "the ucsc|ncbi selects the naming scheme\n" 1>&2
  exit 255
fi

export ncbiUcsc=$1
export dbPrefix=$2
export chromSizes=$3
export agpDirectory=$4
export bbiDir=$5

if [ "${ncbiUcsc}" != "ncbi" ]; then
zcat "${agpDirectory}"/*.agp.gz | grep -v "^#" | awk '$5 != "N" && $5 != "U"' \
   | awk '{printf "%s\t%d\t%d\t%s\t0\t%s\n", $1, $2-1, $3, $6, $9}' \
      | sort -k1,1 -k2,2n > ${bbiDir}/${dbPrefix}.assembly.bed

zcat "${agpDirectory}"/*.agp.gz | grep -v "^#" | awk '$5 == "N" || $5 == "U"' \
   | awk '{printf "%s\t%d\t%d\t%s\n", $1, $2-1, $3, $8}' \
      | sort -k1,1 -k2,2n > ${bbiDir}/${dbPrefix}.gap.bed

if [ -s ${bbiDir}/${dbPrefix}.assembly.bed ]; then
  bedToBigBed -extraIndex=name -verbose=0 \
    ${bbiDir}/${dbPrefix}.assembly.bed ${chromSizes} \
      ${bbiDir}/${dbPrefix}.assembly.bb
fi
if [ -s ${bbiDir}/${dbPrefix}.gap.bed ]; then
  bedToBigBed -verbose=0 ${bbiDir}/${dbPrefix}.gap.bed ${chromSizes} \
      ${bbiDir}/${dbPrefix}.gap.bb
fi
rm -f ${bbiDir}/${dbPrefix}.assembly.bed ${bbiDir}/${dbPrefix}.gap.bed

else

zcat "${agpDirectory}"/*.agp.ncbi.gz | grep -v "^#" | awk '$5 != "N" && $5 != "U"' \
   | awk '{printf "%s\t%d\t%d\t%s\t0\t%s\n", $1, $2-1, $3, $6, $9}' \
      | sort -k1,1 -k2,2n > ${bbiDir}/${dbPrefix}.assembly.ncbi.bed

zcat "${agpDirectory}"/*.agp.ncbi.gz | grep -v "^#" | awk '$5 == "N" || $5 == "U"' \
   | awk '{printf "%s\t%d\t%d\t%s\n", $1, $2-1, $3, $8}' \
      | sort -k1,1 -k2,2n > ${bbiDir}/${dbPrefix}.gap.ncbi.bed

if [ -s ${bbiDir}/${dbPrefix}.assembly.ncbi.bed ]; then
  bedToBigBed -extraIndex=name -verbose=0 \
    ${bbiDir}/${dbPrefix}.assembly.ncbi.bed ${chromSizes} \
      ${bbiDir}/${dbPrefix}.assembly.ncbi.bb
  /hive/data/inside/ncbi/genomes/refseq/scripts/nameToIx.pl \
     ${bbiDir}/${dbPrefix}.assembly.ncbi.bed | sort -u \
        > "${agpDirectory}/assembly.ix.txt"
  if [ -s "${agpDirectory}/assembly.ix.txt" ]; then
    ixIxx "${agpDirectory}/assembly.ix.txt" \
      ${agpDirectory}/${dbPrefix}.assembly.ix \
       ${agpDirectory}/${dbPrefix}.assembly.ixx
  fi
fi
if [ -s ${bbiDir}/${dbPrefix}.gap.ncbi.bed ]; then
  bedToBigBed -verbose=0 ${bbiDir}/${dbPrefix}.gap.ncbi.bed ${chromSizes} \
      ${bbiDir}/${dbPrefix}.gap.ncbi.bb
fi
rm -f "${bbiDir}/${dbPrefix}.assembly.ncbi.bed" \
    "${bbiDir}/${dbPrefix}.gap.ncbi.bed" "${agpDirectory}/assembly.ix.txt"

fi

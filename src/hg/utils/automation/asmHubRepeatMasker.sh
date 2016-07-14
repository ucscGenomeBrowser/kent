#!/bin/bash

# asmHubRepeatMasker.sh - process a specified *.fa.out.gz file into a set
#                         of bigBed files for assembly hub display
#                         
# fail on any error:
set -beEu -o pipefail

if [ $# -ne 3 ]; then
  printf "%s\n" "usage: asmHubRepeatMasker.sh <asmId> <pathTo/*fa.out.gz> <destinationDir/>" 1>&2
  exit 255
fi

export dateStamp=`date "+%FT%T %s"`

# /hive/data/outside/ncbi/genomes/genbank/vertebrate_mammalian/Equus_caballus/latest_assembly_versions/GCA_000002305.1_EquCab2.0/GCA_000002305.1_EquCab2.0_rm.out.gz
# /hive/data/inside/ncbi/genomes/genbank/vertebrate_mammalian/Equus_caballus/latest_assembly_versions/GCA_000002305.1_EquCab2.0

export asmId=$1
export rmOutFile=$2
export destDir=$3

if [ -d "${destDir}" ]; then
  cd "${destDir}"
  rm -fr classBed rmskClass ${asmId}.rmsk.tab bbi/*.rmsk.*.bb \
       bbi/*.rmsk.*.bb
  mkdir classBed rmskClass
  rm -f ${asmId}.rm.out
  mkdir -p bbi
  dateStamp=`date "+%FT%T %s"`
  printf "# %s processing %s\n" "${dateStamp}" "${rmOutFile}" 1>&2
  export rmOutTmp=`mktemp -p /dev/shm rmskProcess.${asmId}.XXXXX`
  printf "%s\n" '   SW  perc perc perc  query      position in query           matching       repeat              position in  repeat
score  div. del. ins.  sequence    begin     end    (left)    repeat         class/family         begin  end (left)   ID
' > "${rmOutTmp}"
  zcat "${rmOutFile}" | headRest 3 stdin >> "${rmOutTmp}"
  hgLoadOut -verbose=2 -tabFile=${asmId}.rmsk.tab -table=rmsk -nosplit test "${rmOutTmp}" 2> rmsk.bad.records.txt
  dateStamp=`date "+%FT%T %s"`
  printf "# %s splitting into categories %s\n" "${dateStamp}" "${asmId}.rmsk.tab" 1>&2
  sort -k12,12 ${asmId}.rmsk.tab \
    | splitFileByColumn -ending=tab  -col=12 -tab stdin rmskClass
  for T in SINE LINE LTR DNA Simple Low_complexity Satellite
  do
    fileCount=`(ls rmskClass/${T}*.tab 2> /dev/null || true) | wc -l`
    if [ "$fileCount" -gt 0 ]; then
       dateStamp=`date "+%FT%T %s"`
       printf "# %s bedToBigBed on: rmskClass/%s*.tab " "${dateStamp}" "${T}"
       ls rmskClass/${T}*.tab | xargs echo
       $HOME/kent/src/hg/utils/automation/rmskBed6+10.pl rmskClass/${T}*.tab \
        | sort -k1,1 -k2,2n > classBed/${asmId}.rmsk.${T}.bed
       bedToBigBed -tab -type=bed6+10 -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
         classBed/${asmId}.rmsk.${T}.bed ../../$asmId.chrom.sizes \
           bbi/${asmId}.rmsk.${T}.bb
    fi
  done
  fileCount=`(ls rmskClass/*RNA.tab 2> /dev/null || true) | wc -l`
  if [ "$fileCount" -gt 0 ]; then
    dateStamp=`date "+%FT%T %s"`
    printf "# %s bedToBigBed on rmskClass/*RNA.tab: " "${dateStamp}"
    ls rmskClass/*RNA.tab | xargs echo
    $HOME/kent/src/hg/utils/automation/rmskBed6+10.pl rmskClass/*RNA.tab \
       | sort -k1,1 -k2,2n > classBed/${asmId}.rmsk.RNA.bed
    bedToBigBed -tab -type=bed6+10 -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
       classBed/${asmId}.rmsk.RNA.bed ../../$asmId.chrom.sizes \
          bbi/${asmId}.rmsk.RNA.bb
  fi
  dateStamp=`date "+%FT%T %s"`
  printf "# %s checking ls rmskClass/*.tab | egrep -v \"/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab\" " "${dateStamp}"
  otherCount=`(ls rmskClass/*.tab 2> /dev/null || true) | (egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab" || true) | wc -l`
  if [ "${otherCount}" -gt 0 ]; then
    dateStamp=`date "+%FT%T %s"`
    printf "# %s bedToBigBed on: " "${dateStamp}"
    ls rmskClass/*.tab | egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab" | xargs echo | sed -e 's#rmskClass/##g;'
    $HOME/kent/src/hg/utils/automation/rmskBed6+10.pl `ls rmskClass/*.tab | egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab" | sed -e 's/^/"/; s/$/"/;'|xargs echo` \
        | sort -k1,1 -k2,2n > classBed/${asmId}.rmsk.Other.bed
    bedToBigBed -tab -type=bed6+10 -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
      classBed/${asmId}.rmsk.Other.bed ../../$asmId.chrom.sizes \
        bbi/${asmId}.rmsk.Other.bb
  fi

  export bbiCount=`for F in bbi/*.rmsk.*.bb; do bigBedInfo $F | grep itemCount; done | awk '{print $NF}' | sed -e 's/,//g' | ave stdin | grep total | awk '{print $2}' | sed -e 's/.000000//'`

  export firstTabCount=`cat ${asmId}.rmsk.tab | wc -l`
  export splitTabCount=`cat rmskClass/*.tab | wc -l`

  if [ "$firstTabCount" -ne $bbiCount ]; then
     printf "%s.rmsk.tab count: %d, split class tab file count: %d, bbi class item count: %d\n" "${asmId}" $firstTabCount $splitTabCount $bbiCount 1>&2
     printf "ERROR: did not account for all items in rmsk class bbi construction\n" 1>&2
     exit 255
  fi
  wc -l classBed/*.bed > ${asmId}.rmsk.class.profile.txt
  wc -l rmskClass/*.tab >> ${asmId}.rmsk.class.profile.txt
  rm -fr rmskClass classBed ${asmId}.rmsk.tab
  rm -f "${rmOutTmp}"
else
  printf "ERROR: can not find destination directory '%s'\n" "${destDir}" 1>&2
  exit 255
fi

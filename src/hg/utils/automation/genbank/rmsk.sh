#!/bin/bash

# rmsk.sh - process the *_rm.out.gz file into a set of bigBed files
#               run up both ncbi and ucsc files

# fail on any error:
set -beEu -o pipefail

if [ $# -ne 2 ]; then
  printf "%s\n" "usage: rmsk.sh <outside/pathTo/*_rm.out.gz> <inside/destinationDir/>" 1>&2
  exit 255
fi

export dateStamp=`date "+%FT%T %s"`

# /hive/data/outside/ncbi/genomes/genbank/vertebrate_mammalian/Equus_caballus/latest_assembly_versions/GCA_000002305.1_EquCab2.0/GCA_000002305.1_EquCab2.0_rm.out.gz
# /hive/data/inside/ncbi/genomes/genbank/vertebrate_mammalian/Equus_caballus/latest_assembly_versions/GCA_000002305.1_EquCab2.0

export rmOutFile=$1
export destDir=$2
export baseFile=`basename "${rmOutFile}"`
export accessionAsmName=`echo $baseFile | sed -e 's/_rm.out.gz//;'`

if [ -d "${destDir}" ]; then
  cd "${destDir}"
  rm -fr classBed rmskClass ${accessionAsmName}.rmsk.tab bbi/*.rmsk.*.ncbi.bb \
       bbi/*.rmsk.*.bb
  mkdir classBed rmskClass
  rm -f ${accessionAsmName}.rm.out
  printf "# %s processing %s\n" "${dateStamp}" "${rmOutFile}" 1>&2
  export rmOutTmp=`mktemp -p /dev/shm rmskProcess.${accessionAsmName}.XXXXX`
  printf "%s\n" '   SW  perc perc perc  query      position in query           matching       repeat              position in  repeat
score  div. del. ins.  sequence    begin     end    (left)    repeat         class/family         begin  end (left)   ID
' > "${rmOutTmp}"
  zcat "${rmOutFile}" | headRest 3 stdin >> "${rmOutTmp}"
  hgLoadOut -verbose=2 -tabFile=${accessionAsmName}.rmsk.tab -table=rmsk -nosplit test "${rmOutTmp}" 2> rmsk.bad.records.txt
  printf "# %s splitting into categories %s\n" "${dateStamp}" "${accessionAsmName}.rmsk.tab" 1>&2
  sort -k12,12 ${accessionAsmName}.rmsk.tab \
    | splitFileByColumn -ending=tab  -col=12 -tab stdin rmskClass
  for T in SINE LINE LTR DNA Simple Low_complexity Satellite
  do
    fileCount=`(ls rmskClass/${T}*.tab 2> /dev/null || true) | wc -l`
    if [ "$fileCount" -gt 0 ]; then
       printf "# %s bedToBigBed on: rmskClass/%s*.tab " "${dateStamp}" "${T}"
       ls rmskClass/${T}*.tab | xargs echo
       $HOME/kent/src/hg/utils/automation/rmskBed6+10.pl rmskClass/${T}*.tab \
        | sort -k1,1 -k2,2n > classBed/${accessionAsmName}.rmsk.${T}.ncbi.bed
       bedToBigBed -tab -type=bed6+10 -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
         classBed/${accessionAsmName}.rmsk.${T}.ncbi.bed *.ncbi.chrom.sizes \
           bbi/${accessionAsmName}.rmsk.${T}.ncbi.bb
       if [ -s "${accessionAsmName}.ncbiToUcsc.lift" ]; then
        liftUp -type=.bed stdout ${accessionAsmName}.ncbiToUcsc.lift error \
          classBed/${accessionAsmName}.rmsk.${T}.ncbi.bed | sort -k1,1 -k2,2n \
            > classBed/${accessionAsmName}.rmsk.${T}.bed
        bedToBigBed -tab -type=bed6+10 \
          -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
            classBed/${accessionAsmName}.rmsk.${T}.bed *.ucsc.chrom.sizes \
               bbi/${accessionAsmName}.rmsk.${T}.bb
       fi
    fi
  done
  fileCount=`(ls rmskClass/*RNA.tab 2> /dev/null || true) | wc -l`
  if [ "$fileCount" -gt 0 ]; then
    printf "# %s bedToBigBed on rmskClass/*RNA.tab: " "${dateStamp}"
    ls rmskClass/*RNA.tab | xargs echo
    $HOME/kent/src/hg/utils/automation/rmskBed6+10.pl rmskClass/*RNA.tab \
       | sort -k1,1 -k2,2n > classBed/${accessionAsmName}.rmsk.RNA.ncbi.bed
    bedToBigBed -tab -type=bed6+10 -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
       classBed/${accessionAsmName}.rmsk.RNA.ncbi.bed *.ncbi.chrom.sizes \
          bbi/${accessionAsmName}.rmsk.RNA.ncbi.bb
    if [ -s "${accessionAsmName}.ncbiToUcsc.lift" ]; then
      liftUp -type=.bed stdout ${accessionAsmName}.ncbiToUcsc.lift error \
        classBed/${accessionAsmName}.rmsk.RNA.ncbi.bed | sort -k1,1 -k2,2n \
          > classBed/${accessionAsmName}.rmsk.RNA.bed
      bedToBigBed -tab -type=bed6+10 \
        -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
          classBed/${accessionAsmName}.rmsk.RNA.bed *.ucsc.chrom.sizes \
            bbi/${accessionAsmName}.rmsk.RNA.bb
    fi
  fi
  printf "# %s checking ls rmskClass/*.tab | egrep -v \"/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab\" " "${dateStamp}"
  otherCount=`(ls rmskClass/*.tab 2> /dev/null || true) | (egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab" || true) | wc -l`
  if [ "${otherCount}" -gt 0 ]; then
    printf "# %s bedToBigBed on: " "${dateStamp}"
    ls rmskClass/*.tab | egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab" | xargs echo | sed -e 's#rmskClass/##g;'
    $HOME/kent/src/hg/utils/automation/rmskBed6+10.pl `ls rmskClass/*.tab | egrep -v "/SIN|/LIN|/LT|/DN|/Simple|/Low_complexity|/Satellit|RNA.tab" | sed -e 's/^/"/; s/$/"/;'|xargs echo` \
        | sort -k1,1 -k2,2n > classBed/${accessionAsmName}.rmsk.Other.ncbi.bed
    bedToBigBed -tab -type=bed6+10 -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
      classBed/${accessionAsmName}.rmsk.Other.ncbi.bed *.ncbi.chrom.sizes \
        bbi/${accessionAsmName}.rmsk.Other.ncbi.bb
    if [ -s "${accessionAsmName}.ncbiToUcsc.lift" ]; then
      liftUp -type=.bed stdout ${accessionAsmName}.ncbiToUcsc.lift error \
        classBed/${accessionAsmName}.rmsk.Other.ncbi.bed | sort -k1,1 -k2,2n \
          > classBed/${accessionAsmName}.rmsk.Other.bed
      bedToBigBed -tab -type=bed6+10 \
        -as=$HOME/kent/src/hg/lib/rmskBed6+10.as \
          classBed/${accessionAsmName}.rmsk.Other.bed *.ucsc.chrom.sizes \
            bbi/${accessionAsmName}.rmsk.Other.bb
    fi
  fi

  export bbiCount=`for F in bbi/*.rmsk.*.ncbi.bb; do bigBedInfo $F | grep itemCount; done | awk '{print $NF}' | sed -e 's/,//g' | ave stdin | grep total | awk '{print $2}' | sed -e 's/.000000//'`

  export firstTabCount=`cat ${accessionAsmName}.rmsk.tab | wc -l`
  export splitTabCount=`cat rmskClass/*.tab | wc -l`

  if [ "$firstTabCount" -ne $bbiCount ]; then
     printf "%s.rmsk.tab count: %d, split class tab file count: %d, bbi class item count: %d\n" "${accessionAsmName}" $firstTabCount $splitTabCount $bbiCount 1>&2
     printf "ERROR: did not account for all items in rmsk class bbi construction\n" 1>&2
     exit 255
  fi
  wc -l classBed/*.ncbi.bed > ${accessionAsmName}.rmsk.class.profile.txt
  wc -l rmskClass/*.tab >> ${accessionAsmName}.rmsk.class.profile.txt
  rm -fr rmskClass classBed ${accessionAsmName}.rmsk.tab
  rm -f "${rmOutTmp}"
else
  printf "ERROR: can not find destination directory '%s'\n" "${destDir}" 1>&2
  exit 255
fi

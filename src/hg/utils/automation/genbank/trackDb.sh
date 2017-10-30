#!/bin/bash

set -beEu -o pipefail

if [ $# -lt 2 ]; then
  printf "usage: trackDb.sh [ncbi|ucsc] <dbPrefix> <inside/pathTo/assembly files> > trackDb.txt\n" 1>&2
  printf "expecting to find *.ucsc.2bit and bbi/ files at given path\n" 1>&2
  printf "the ncbi|ucsc selects the naming scheme\n" 1>&2
  exit 255
fi

export ncbiUcsc=$1
export dbPrefix=$2
export buildDir=$3

export suffix=""
if [ "${ncbiUcsc}" = "ncbi" ]; then
  suffix=".ncbi"
fi

# may or may not have a searchTrix for assembly, assume none
export searchTrix=""
# check to see if there is a searchTrix assembly index
if [ -s ${buildDir}/${dbPrefix}.assembly.ix ]; then
  searchTrix="
searchTrix ${dbPrefix}.assembly.ix"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.assembly${suffix}.bb ]; then
printf "track assembly
longLabel Assembly 
shortLabel Assembly 
priority 10
visibility pack
colorByStrand 150,100,30 230,170,40
color 150,100,30
altColor 230,170,40
bigDataUrl bbi/%s.assembly%s.bb
type bigBed 6
html %s.assembly
searchIndex name%s
url https://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database
group map\n\n" "${dbPrefix}" "${suffix}" "${dbPrefix}" "${searchTrix}"
fi

# may or may not have a searchTrix for ncbiGene, assume none
searchTrix=""
# check to see if there is a index for ncbiGene
if [ -s ${buildDir}/${dbPrefix}.ncbiGene.ix ]; then
  searchTrix="
searchTrix ${dbPrefix}.ncbiGene.ix"
fi

if [ "${ncbiUcsc}" = "ncbi" ]; then

  if [ -s ${buildDir}/bbi/${dbPrefix}.ncbiGene${suffix}.bb ]; then
  printf "track ncbiGene
longLabel ncbiGene - gene predictions delivered with assembly from NCBI
shortLabel ncbiGene
priority 12
visibility pack
color 0,80,150
altColor 150,80,0
colorByStrand 0,80,150 150,80,0
bigDataUrl bbi/%s.ncbiGene%s.bb
type bigGenePred
html %s.ncbiGene
searchIndex name%s
url https://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database
group genes\n\n" "${dbPrefix}" "${suffix}" "${dbPrefix}" "${searchTrix}"
  fi

else

  if [ -s ${buildDir}/bbi/${dbPrefix}.ncbiGene.ucsc.bb ]; then
  printf "track ncbiGene
longLabel ncbiGene - gene predictions delivered with assembly from NCBI
shortLabel ncbiGene
priority 12
visibility pack
color 0,80,150
altColor 150,80,0
colorByStrand 0,80,150 150,80,0
bigDataUrl bbi/%s.ncbiGene%s.bb
type bigGenePred
html %s.ncbiGene
searchIndex name%s
url https://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database
group genes\n\n" "${dbPrefix}" ".ucsc" "${dbPrefix}" "${searchTrix}"
  fi

fi


if [ -s ${buildDir}/bbi/${dbPrefix}.gap${suffix}.bb ]; then
printf "track gap
longLabel Gap 
shortLabel Gap 
priority 11
visibility dense
color 0,0,0 
bigDataUrl bbi/%s.gap%s.bb
type bigBed 4
group map
html %s.gap\n\n" "${dbPrefix}" "${suffix}" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.gc5Base${suffix}.bw ]; then
printf "track gc5Base
shortLabel GC Percent
longLabel GC Percent in 5-Base Windows
group map
priority 23.5
visibility full
autoScale Off
maxHeightPixels 128:36:16
graphTypeDefault Bar
gridDefault OFF
windowingFunction Mean
color 0,0,0
altColor 128,128,128
viewLimits 30:70
type bigWig 0 100
bigDataUrl bbi/%s.gc5Base%s.bw
html %s.gc5Base\n\n" "${dbPrefix}" "${suffix}" "${dbPrefix}"
fi

if [ "${ncbiUcsc}" = "ncbi" ]; then

export rmskCount=`(ls ${buildDir}/bbi/${dbPrefix}.rmsk.*.ncbi.bb | wc -l) || true`

else

export rmskCount=`(ls ${buildDir}/bbi/${dbPrefix}.rmsk.*.bb | wc -l) || true`

fi

if [ "${rmskCount}" -gt 0 ]; then
printf "track repeatMasker
compositeTrack on
shortLabel RepeatMasker
longLabel Repeating Elements by RepeatMasker
group varRep
priority 149.1
visibility dense
type bed 3 .
noInherit on
html %s.repeatMasker\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.SINE${suffix}.bb ]; then
printf "    track repeatMaskerSINE
    parent repeatMasker
    shortLabel SINE
    longLabel SINE Repeating Elements by RepeatMasker
    priority 1
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.SINE%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.LINE${suffix}.bb ]; then
printf "    track repeatMaskerLINE
    parent repeatMasker
    shortLabel LINE
    longLabel LINE Repeating Elements by RepeatMasker
    priority 2
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.LINE%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.LTR${suffix}.bb ]; then
printf "    track repeatMaskerLTR
    parent repeatMasker
    shortLabel LTR
    longLabel LTR Repeating Elements by RepeatMasker
    priority 3
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.LTR%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.DNA${suffix}.bb ]; then
printf "    track repeatMaskerDNA
    parent repeatMasker
    shortLabel DNA
    longLabel DNA Repeating Elements by RepeatMasker
    priority 4
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.DNA%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.Simple${suffix}.bb ]; then
printf "    track repeatMaskerSimple
    parent repeatMasker
    shortLabel Simple
    longLabel Simple Repeating Elements by RepeatMasker
    priority 5
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Simple%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.Low_complexity${suffix}.bb ]; then
printf "    track repeatMaskerLowComplexity
    parent repeatMasker
    shortLabel Low Complexity
    longLabel Low Complexity Repeating Elements by RepeatMasker
    priority 6
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Low_complexity%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.Satellite${suffix}.bb ]; then
printf "    track repeatMaskerSatellite
    parent repeatMasker
    shortLabel Satellite
    longLabel Satellite Repeating Elements by RepeatMasker
    priority 7
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Satellite%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.RNA${suffix}.bb ]; then
printf "    track repeatMaskerRNA
    parent repeatMasker
    shortLabel RNA
    longLabel RNA Repeating Elements by RepeatMasker
    priority 8
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.RNA%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.Other${suffix}.bb ]; then
printf "    track repeatMaskerOther
    parent repeatMasker
    shortLabel Other
    longLabel Other Repeating Elements by RepeatMasker
    priority 9
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Other%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

###################################################################
# CpG Islands composite
if [ -s ${buildDir}/bbi/${dbPrefix}.cpgIslandExtUnmasked${suffix}.bb -o -s ${buildDir}/bbi/${dbPrefix}.cpgIslandExt${suffix}.bb ]; then
printf "track cpgIslands
compositeTrack on
shortLabel CpG Islands
longLabel CpG Islands (Islands < 300 Bases are Light Green)
group regulation
priority 90
visibility pack
type bed 3 .
noInherit on
html %s.cpgIslands\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.cpgIslandExt${suffix}.bb ]; then
printf "    track cpgIslandExt
    parent cpgIslands
    shortLabel CpG Islands
    longLabel CpG Islands (Islands < 300 Bases are Light Green)
    priority 1
    type bigBed 4 +
    bigDataUrl bbi/%s.cpgIslandExt%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.cpgIslandExtUnmasked${suffix}.bb ]; then
printf "    track cpgIslandExtUnmasked
    parent cpgIslands
    shortLabel Unmasked CpG
    longLabel CpG Islands on All Sequence (Islands < 300 Bases are Light Green)
    priority 2
    type bigBed 4 +
    bigDataUrl bbi/%s.cpgIslandExtUnmasked%s.bb\n\n" "${dbPrefix}" "${suffix}"
fi

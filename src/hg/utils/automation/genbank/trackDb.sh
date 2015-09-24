#!/bin/bash

set -beEu -o pipefail

if [ $# -lt 2 ]; then
  echo "usage: trackDb.sh <dbPrefix> <pathTo/assembly files> > trackDb.txt" 1>&2
  echo "expecting to find *.ucsc.2bit and bbi/ files at given path" 1>&2
  exit 255
fi

export dbPrefix=$1
export buildDir=$2

if [ -s ${buildDir}/bbi/${dbPrefix}.assembly.bb ]; then
printf "track assembly
longLabel Assembly 
shortLabel Assembly 
priority 10
visibility pack
colorByStrand 150,100,30 230,170,40
color 150,100,30
altColor 230,170,40
bigDataUrl bbi/%s.assembly.bb
type bigBed 6
html ../trackDescriptions/assembly
url http://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database
group map\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.gap.bb ]; then
printf "track gap
longLabel Gap 
shortLabel Gap 
priority 11
visibility dense
color 0,0,0 
bigDataUrl bbi/%s.gap.bb
type bigBed 4
group map
html ../trackDescriptions/gap\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.gc5Base.bw ]; then
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
bigDataUrl bbi/%s.gc5Base.bw
html ../trackDescriptions/gc5Base\n\n" "${dbPrefix}"
fi

export rmskCount=`(ls ${buildDir}/bbi/${dbPrefix}.rmsk.*.bb | wc -l) || true`

# if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.SINE.bb -o -s ${buildDir}/bbi/${dbPrefix}.rmsk.LINE.bb ]; then

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
html ../trackDescriptions/repeatMasker\n\n"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.SINE.bb ]; then
printf "    track repeatMaskerSINE
    parent repeatMasker
    shortLabel SINE
    longLabel SINE Repeating Elements by RepeatMasker
    priority 1
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.SINE.bb\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.LINE.bb ]; then
printf "    track repeatMaskerLINE
    parent repeatMasker
    shortLabel LINE
    longLabel LINE Repeating Elements by RepeatMasker
    priority 2
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.LINE.bb\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.LTR.bb ]; then
printf "    track repeatMaskerLTR
    parent repeatMasker
    shortLabel LTR
    longLabel LTR Repeating Elements by RepeatMasker
    priority 3
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.LTR.bb\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.DNA.bb ]; then
printf "    track repeatMaskerDNA
    parent repeatMasker
    shortLabel DNA
    longLabel DNA Repeating Elements by RepeatMasker
    priority 4
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.DNA.bb\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.Simple.bb ]; then
printf "    track repeatMaskerSimple
    parent repeatMasker
    shortLabel Simple
    longLabel Simple Repeating Elements by RepeatMasker
    priority 5
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Simple.bb\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.Low_complexity.bb ]; then
printf "    track repeatMaskerLowComplexity
    parent repeatMasker
    shortLabel Low Complexity
    longLabel Low Complexity Repeating Elements by RepeatMasker
    priority 6
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Low_complexity.bb\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.Satellite.bb ]; then
printf "    track repeatMaskerSatellite
    parent repeatMasker
    shortLabel Satellite
    longLabel Satellite Repeating Elements by RepeatMasker
    priority 7
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Satellite.bb\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.RNA.bb ]; then
printf "    track repeatMaskerRNA
    parent repeatMasker
    shortLabel RNA
    longLabel RNA Repeating Elements by RepeatMasker
    priority 8
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.RNA.bb\n\n" "${dbPrefix}"
fi

if [ -s ${buildDir}/bbi/${dbPrefix}.rmsk.Other.bb ]; then
printf "    track repeatMaskerOther
    parent repeatMasker
    shortLabel Other
    longLabel Other Repeating Elements by RepeatMasker
    priority 9
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Other.bb\n\n" "${dbPrefix}"
fi

#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 2 ]; then
  printf "usage: trackDb.sh <asmId> <pathTo/assembly hub build directory> > trackDb.txt\n" 1>&2
  printf "expecting to find *.ucsc.2bit and bbi/ files at given path\n" 1>&2
  printf "the ncbi|ucsc selects the naming scheme\n" 1>&2
  exit 255
fi

export asmId=$1
export buildDir=$2

mkdir -p $buildDir/bbi
mkdir -p $buildDir/ixIxx

# may or may not have a searchTrix for assembly, assume none
export searchTrix=""
# check to see if there is a searchTrix assembly index
if [ -s ${buildDir}/trackData/assemblyGap/${asmId}.assembly.ix ]; then
  rm -f $buildDir/ixIxx/${asmId}.assembly.ix*
  ln -s ${buildDir}/trackData/assemblyGap/${asmId}.assembly.ix $buildDir/ixIxx
  ln -s ${buildDir}/trackData/assemblyGap/${asmId}.assembly.ixx $buildDir/ixIxx
  searchTrix="
searchTrix ixIxx/${asmId}.assembly.ix"
fi

if [ -s ${buildDir}/trackData/assemblyGap/${asmId}.assembly.bb ]; then
rm -f $buildDir/bbi/${asmId}.assembly.bb
ln -s $buildDir/trackData/assemblyGap/${asmId}.assembly.bb $buildDir/bbi/${asmId}.assembly.bb
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
html html/%s.assembly
searchIndex name%s
url http://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database
group map\n\n" "${asmId}" "${asmId}" "${searchTrix}"
fi

if [ -s ${buildDir}/trackData/assemblyGap/${asmId}.gap.bb ]; then
rm -f $buildDir/bbi/${asmId}.gap.bb
ln -s $buildDir/trackData/assemblyGap/${asmId}.gap.bb $buildDir/bbi/${asmId}.gap.bb
printf "track gap
longLabel Gap 
shortLabel Gap 
priority 11
visibility dense
color 0,0,0 
bigDataUrl bbi/%s.gap.bb
type bigBed 4
group map
html html/%s.gap\n\n" "${asmId}" "${asmId}"
fi

if [ -s ${buildDir}/trackData/gc5Base/${asmId}.gc5Base.bw ]; then
rm -f $buildDir/bbi/${asmId}.gc5Base.bw
ln -s $buildDir/trackData/gc5Base/${asmId}.gc5Base.bw $buildDir/bbi/${asmId}.gc5Base.bw
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
html html/%s.gc5Base\n\n" "${asmId}" "${asmId}"
fi

# see if there are repeatMasker bb files
export rmskCount=`(ls $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.*.bb | wc -l) || true`

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
html html/%s.repeatMasker\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.SINE.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.SINE.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.SINE.bb $buildDir/bbi/${asmId}.rmsk.SINE.bb
printf "    track repeatMaskerSINE
    parent repeatMasker
    shortLabel SINE
    longLabel SINE Repeating Elements by RepeatMasker
    priority 1
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.SINE.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.LINE.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.LINE.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.LINE.bb $buildDir/bbi/${asmId}.rmsk.LINE.bb
printf "    track repeatMaskerLINE
    parent repeatMasker
    shortLabel LINE
    longLabel LINE Repeating Elements by RepeatMasker
    priority 2
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.LINE.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.LTR.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.LTR.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.LTR.bb $buildDir/bbi/${asmId}.rmsk.LTR.bb
printf "    track repeatMaskerLTR
    parent repeatMasker
    shortLabel LTR
    longLabel LTR Repeating Elements by RepeatMasker
    priority 3
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.LTR.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.DNA.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.DNA.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.DNA.bb $buildDir/bbi/${asmId}.rmsk.DNA.bb
printf "    track repeatMaskerDNA
    parent repeatMasker
    shortLabel DNA
    longLabel DNA Repeating Elements by RepeatMasker
    priority 4
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.DNA.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.Simple.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.Simple.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.Simple.bb $buildDir/bbi/${asmId}.rmsk.Simple.bb
printf "    track repeatMaskerSimple
    parent repeatMasker
    shortLabel Simple
    longLabel Simple Repeating Elements by RepeatMasker
    priority 5
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Simple.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.Low_complexity.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.Low_complexity.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.Low_complexity.bb $buildDir/bbi/${asmId}.rmsk.Low_complexity.bb
printf "    track repeatMaskerLowComplexity
    parent repeatMasker
    shortLabel Low Complexity
    longLabel Low Complexity Repeating Elements by RepeatMasker
    priority 6
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Low_complexity.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.Satellite.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.Satellite.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.Satellite.bb $buildDir/bbi/${asmId}.rmsk.Satellite.bb
printf "    track repeatMaskerSatellite
    parent repeatMasker
    shortLabel Satellite
    longLabel Satellite Repeating Elements by RepeatMasker
    priority 7
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Satellite.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.RNA.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.RNA.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.RNA.bb $buildDir/bbi/${asmId}.rmsk.RNA.bb
printf "    track repeatMaskerRNA
    parent repeatMasker
    shortLabel RNA
    longLabel RNA Repeating Elements by RepeatMasker
    priority 8
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.RNA.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.Other.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.Other.bb
ln -s $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.Other.bb $buildDir/bbi/${asmId}.rmsk.Other.bb
printf "    track repeatMaskerOther
    parent repeatMasker
    shortLabel Other
    longLabel Other Repeating Elements by RepeatMasker
    priority 9
    spectrum on
    maxWindowToDraw 10000000
    colorByStrand 50,50,150 150,50,50
    type bigBed 6 +
    bigDataUrl bbi/%s.rmsk.Other.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/simpleRepeat/simpleRepeat.bb ]; then
rm -f $buildDir/bbi/${asmId}.simpleRepeat.bb
ln -s $buildDir/trackData/simpleRepeat/simpleRepeat.bb $buildDir/bbi/${asmId}.simpleRepeat.bb
printf "track simpleRepeat
shortLabel Simple Repeats
longLabel Simple Tandem Repeats by TRF
group varRep
priority 149.3
visibility dense
type bigBed 4 +
bigDataUrl bbi/%s.simpleRepeat.bb
html html/%s.simpleRepeat\n\n" "${asmId}" "${asmId}"
fi


# may or may not have a searchTrix for ncbiGene, assume none
searchTrix=""
# check to see if there is a index for ncbiGene
if [ -s ${buildDir}/trackData/geneTrack/ucsc.NZO_HlLtJ.ix ]; then
  searchTrix="
searchTrix ixIxx/$asmId.geneTrack.ix"
fi

if [ -s ${buildDir}/trackData/geneTrack/ucsc.NZO_HlLtJ.bb ]; then
rm -f $buildDir/bbi/${asmId}.geneTrack.bb
rm -f $buildDir/ixIxx/${asmId}.geneTrack.ix
rm -f $buildDir/ixIxx/${asmId}.geneTrack.ixx
ln -s $buildDir/trackData/geneTrack/ucsc.NZO_HlLtJ.bb $buildDir/bbi/${asmId}.geneTrack.bb
ln -s $buildDir/trackData/geneTrack/ucsc.NZO_HlLtJ.ix $buildDir/ixIxx/${asmId}.geneTrack.ix
ln -s $buildDir/trackData/geneTrack/ucsc.NZO_HlLtJ.ixx $buildDir/ixIxx/${asmId}.geneTrack.ixx
  printf "track geneTrack
longLabel geneTrack - gene predictions
shortLabel geneTrack
priority 12
visibility pack
color 0,80,150
altColor 150,80,0
colorByStrand 0,80,150 150,80,0
bigDataUrl bbi/%s.geneTrack.bb
type bigGenePred
html html/%s.geneTrack
searchIndex name%s
group genes\n\n" "${asmId}" "${asmId}" "${searchTrix}"
fi

exit $?

# url http://www.ncbi.nlm.nih.gov/nuccore/\$\$
# urlLabel NCBI Nucleotide database

###################################################################
# CpG Islands composite
if [ -s ${buildDir}/bbi/${asmId}.cpgIslandExtUnmasked${suffix}.bb -o -s ${buildDir}/bbi/${asmId}.cpgIslandExt${suffix}.bb ]; then
printf "track cpgIslands
compositeTrack on
shortLabel CpG Islands
longLabel CpG Islands (Islands < 300 Bases are Light Green)
group regulation
priority 90
visibility pack
type bed 3 .
noInherit on
html %s.cpgIslands\n\n" "${asmId}"
fi

if [ -s ${buildDir}/bbi/${asmId}.cpgIslandExt${suffix}.bb ]; then
printf "    track cpgIslandExt
    parent cpgIslands
    shortLabel CpG Islands
    longLabel CpG Islands (Islands < 300 Bases are Light Green)
    priority 1
    type bigBed 4 +
    bigDataUrl bbi/%s.cpgIslandExt%s.bb\n\n" "${asmId}" "${suffix}"
fi

if [ -s ${buildDir}/bbi/${asmId}.cpgIslandExtUnmasked${suffix}.bb ]; then
printf "    track cpgIslandExtUnmasked
    parent cpgIslands
    shortLabel Unmasked CpG
    longLabel CpG Islands on All Sequence (Islands < 300 Bases are Light Green)
    priority 2
    type bigBed 4 +
    bigDataUrl bbi/%s.cpgIslandExtUnmasked%s.bb\n\n" "${asmId}" "${suffix}"
fi

#!/bin/bash

set -beEux -o pipefail

if [ $# -ne 3 ]; then
  printf "usage: trackDb.sh <genbank|refseq> <asmId> <pathTo/assembly hub build directory> > trackDb.txt\n" 1>&2
  printf "expecting to find *.ucsc.2bit and bbi/ files at given path\n" 1>&2
  printf "the ncbi|ucsc selects the naming scheme\n" 1>&2
  exit 255
fi

export genbankRefseq=$1
export asmId=$2
export buildDir=$3
export hubLinks="/hive/data/genomes/asmHubs/hubLinks"

export scriptDir="$HOME/kent/src/hg/utils/automation"

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
visibility pack
colorByStrand 150,100,30 230,170,40
color 150,100,30
altColor 230,170,40
bigDataUrl bbi/%s.assembly.bb
type bigBed 6
html html/%s.assembly
searchIndex name%s
url https://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database
group map\n\n" "${asmId}" "${asmId}" "${searchTrix}"
$scriptDir/asmHubAssembly.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/$asmId.agp.gz ../hubs/ncbiAssemblies/$genbankRefseq/$asmId > $buildDir/html/$asmId.assembly.html
fi

if [ -s ${buildDir}/trackData/assemblyGap/${asmId}.gap.bb ]; then
rm -f $buildDir/bbi/${asmId}.gap.bb
ln -s $buildDir/trackData/assemblyGap/${asmId}.gap.bb $buildDir/bbi/${asmId}.gap.bb
printf "track gap
longLabel AGP gap
shortLabel Gap (AGP defined)
visibility dense
color 0,0,0
bigDataUrl bbi/%s.gap.bb
type bigBed 4
group map
html html/%s.gap\n\n" "${asmId}" "${asmId}"
$scriptDir/asmHubGap.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/$asmId.agp.gz ../hubs/ncbiAssemblies/$genbankRefseq/$asmId > $buildDir/html/$asmId.gap.html
fi

if [ -s ${buildDir}/trackData/gc5Base/${asmId}.gc5Base.bw ]; then
rm -f $buildDir/bbi/${asmId}.gc5Base.bw
ln -s $buildDir/trackData/gc5Base/${asmId}.gc5Base.bw $buildDir/bbi/${asmId}.gc5Base.bw
printf "track gc5Base
shortLabel GC Percent
longLabel GC Percent in 5-Base Windows
group map
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
$scriptDir/asmHubGc5Percent.pl $asmId $buildDir/html/$asmId.names.tab $buildDir > $buildDir/html/$asmId.gc5Base.html
fi

# see if there are repeatMasker bb files
export rmskCount=`(ls $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.*.bb | wc -l) || true`

if [ "${rmskCount}" -gt 0 ]; then
printf "track repeatMasker
compositeTrack on
shortLabel RepeatMasker
longLabel Repeating Elements by RepeatMasker
group varRep
visibility dense
type bed 3 .
noInherit on
html html/%s.repeatMasker\n\n" "${asmId}"
$scriptDir/asmHubRmsk.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData/repeatMasker/$asmId.rmsk.class.profile.txt > $buildDir/html/$asmId.repeatMasker.html
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
visibility dense
type bigBed 4 +
bigDataUrl bbi/%s.simpleRepeat.bb
html html/%s.simpleRepeat\n\n" "${asmId}" "${asmId}"
$scriptDir/asmHubSimpleRepeat.pl $asmId $buildDir/html/$asmId.names.tab $buildDir > $buildDir/html/$asmId.simpleRepeat.html
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

# url https://www.ncbi.nlm.nih.gov/nuccore/\$\$
# urlLabel NCBI Nucleotide database

###################################################################
# CpG Islands composite
if [ -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb -o -s ${buildDir}/trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.cpgIslandExtUnmasked.bb ${buildDir}/bbi/${asmId}.cpgIslandExt.bb

printf "track cpgIslands
compositeTrack on
shortLabel CpG Islands
longLabel CpG Islands (Islands < 300 Bases are Light Green)
group regulation
visibility pack
type bed 3 .
noInherit on
html html/%s.cpgIslands\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ]; then
ln -s ${buildDir}/trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ${buildDir}/bbi/${asmId}.cpgIslandExt.bb
printf "    track cpgIslandExt
    parent cpgIslands
    shortLabel CpG Islands
    longLabel CpG Islands (Islands < 300 Bases are Light Green)
    priority 1
    type bigBed 4 +
    bigDataUrl bbi/%s.cpgIslandExt.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb ]; then
ln -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb ${buildDir}/bbi/${asmId}.cpgIslandExtUnmasked.bb
printf "    track cpgIslandExtUnmasked
    parent cpgIslands
    shortLabel Unmasked CpG
    longLabel CpG Islands on All Sequence (Islands < 300 Bases are Light Green)
    priority 2
    type bigBed 4 +
    bigDataUrl bbi/%s.cpgIslandExtUnmasked.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb -o -s ${buildDir}/trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ]; then
  $scriptDir/asmHubCpG.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.cpgIslands.html
fi

###################################################################
# windowMasker
if [ -s ${buildDir}/trackData/windowMasker/${asmId}.windowMasker.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.windowMasker.bb
ln -s ${buildDir}/trackData/windowMasker/${asmId}.windowMasker.bb ${buildDir}/bbi/${asmId}.windowMasker.bb

printf "track windowMasker
shortLabel WM + SDust
longLabel Genomic Intervals Masked by WindowMasker + SDust
group varRep
visibility dense
type bigBed 3
bigDataUrl bbi/%s.windowMasker.bb
html html/%s.windowMasker\n\n" "${asmId}" "${asmId}"
$scriptDir/asmHubWindowMasker.pl $asmId $buildDir/html/$asmId.names.tab $buildDir > $buildDir/html/$asmId.windowMasker.html
fi

###################################################################
# allGaps
if [ -s ${buildDir}/trackData/allGaps/${asmId}.allGaps.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.allGaps.bb
ln -s ${buildDir}/trackData/allGaps/${asmId}.allGaps.bb ${buildDir}/bbi/${asmId}.allGaps.bb

printf "track allGaps
shortLabel All Gaps
longLabel All gaps of unknown nucleotides (N's), including AGP annotated gaps
group map
visibility dense
type bigBed 3
bigDataUrl bbi/%s.allGaps.bb
html html/%s.allGaps\n\n" "${asmId}" "${asmId}"
$scriptDir/asmHubAllGaps.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/$asmId.agp.gz ../hubs/ncbiAssemblies/$genbankRefseq/$asmId $buildDir/bbi/$asmId > $buildDir/html/$asmId.allGaps.html
fi

###################################################################
# augustus genes
if [ -s ${buildDir}/trackData/augustus/${asmId}.augustus.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.augustus.bb
ln -s ${buildDir}/trackData/augustus/${asmId}.augustus.bb ${buildDir}/bbi/${asmId}.augustus.bb

printf "track augustus
shortLabel Augustus
longLabel Augustus Gene Predictions
group genes
visibility dense
color 180,0,0
type bigGenePred
bigDataUrl bbi/%s.augustus.bb
html html/%s.augustus\n\n" "${asmId}" "${asmId}"
$scriptDir/asmHubAugustusGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.augustus.html
fi

###################################################################
# gapOverlap
if [ -s ${buildDir}/trackData/gapOverlap/${asmId}.gapOverlap.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.gapOverlap.bb
ln -s ${buildDir}/trackData/gapOverlap/${asmId}.gapOverlap.bb ${buildDir}/bbi/${asmId}.gapOverlap.bb

printf "track gapOverlap
shortLabel Gap Overlaps
longLabel Exactly identical sequence on each side of a gap
group map
visibility hide
type bigBed 12 .
bigDataUrl bbi/%s.gapOverlap.bb
html html/%s.gapOverlap\n\n" "${asmId}" "${asmId}"

$scriptDir/asmHubGapOverlap.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.gapOverlap.html

fi
###################################################################

printf "# Plink: ${buildDir}/trackData/ensGene/process/bbi/${asmId}.ensGene.bb ${buildDir}/bbi/${asmId}.ensGene.bb\n" 1>&2
###################################################################
# Ensembl genes
if [ -s ${buildDir}/trackData/ensGene/process/bbi/${asmId}.ensGene.bb ]; then
printf "# link: ${buildDir}/trackData/ensGene/process/bbi/${asmId}.ensGene.bb ${buildDir}/bbi/${asmId}.ensGene.bb\n" 1>&2
rm -f ${buildDir}/bbi/${asmId}.ensGene.bb
ln -s ${buildDir}/trackData/ensGene/process/bbi/${asmId}.ensGene.bb ${buildDir}/bbi/${asmId}.ensGene.bb

printf "track ensGene
shortLabel Ensembl genes
longLabel Ensembl genes v86
group genes
priority 40
visibility pack
color 150,0,0
type bigBed 12 .
bigDataUrl bbi/%s.ensGene.bb
searchIndex name
searchTrix ixIxx/%s.ensGene.name.ix
html html/%s.ensGene\n\n" "${asmId}" "${asmId}" "${asmId}"
$scriptDir/asmHubEnsGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.ensGene.html
fi

if [ -s ${hubLinks}/${asmId}/rnaSeqData/$asmId.trackDb.txt ]; then
  printf "include rnaSeqData/%s.trackDb.txt\n\n" "${asmId}"
fi

#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 3 ]; then
  printf "usage: trackDb.sh <asmId> <ncbiAsmId> <pathTo/assembly hub build directory> > trackDb.txt\n" 1>&2
  printf "expecting to find *.ucsc.2bit and bbi/ files at given path\n" 1>&2
  printf "asmId may be equal to ncbiAsmId if it is a GenArk build\n" 1>&2
  printf "or asmId might be a default dbName if it is a UCSC style\n" 1>&2
  printf "browser build.\n" 1>&2
  exit 255
fi

export asmId=$1
export ncbiAsmId=$2
export buildDir=$3

export scriptDir="$HOME/kent/src/hg/utils/automation"

export asmType="n/a"

# technique to set variables based on the name in another variable:

if [ -s "$buildDir/dropTracks.list" ]; then
  printf "# reading dropTracks.list\n" 1>&2
  for dropTrack in `cat "$buildDir/dropTracks.list"`
  do
     notTrack="not_${dropTrack}"
#      printf "# %s\n" "${notTrack}" 1>&2
     eval $notTrack="1"
  done
fi

case "${asmId}" in
  GCA_*) asmType="genbank"
    ;;
  GCF_*) asmType="refseq"
    ;;
esac

mkdir -p $buildDir/bbi
mkdir -p $buildDir/ixIxx

# may or may not have a searchTrix for assembly, assume none
export searchTrix=""
# check to see if there is a searchTrix assembly index
if [ -s ${buildDir}/trackData/assemblyGap/${asmId}.assembly.ix ]; then
  rm -f $buildDir/ixIxx/${asmId}.assembly.ix*
  ln -s ../trackData/assemblyGap/${asmId}.assembly.ix $buildDir/ixIxx
  ln -s ../trackData/assemblyGap/${asmId}.assembly.ixx $buildDir/ixIxx
  searchTrix="
searchTrix ixIxx/${asmId}.assembly.ix"
fi

if [ -s ${buildDir}/trackData/assemblyGap/${asmId}.assembly.bb ]; then
rm -f $buildDir/bbi/${asmId}.assembly.bb
ln -s ../trackData/assemblyGap/${asmId}.assembly.bb $buildDir/bbi/${asmId}.assembly.bb
printf "track assembly
longLabel Assembly
shortLabel Assembly
visibility hide
colorByStrand 150,100,30 230,170,40
color 150,100,30
altColor 230,170,40
bigDataUrl bbi/%s.assembly.bb
type bigBed 6
html html/%s.assembly
searchIndex name%s
url https://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database:
group map\n\n" "${asmId}" "${asmId}" "${searchTrix}"
$scriptDir/asmHubAssembly.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/$asmId.agp.gz https://hgdownload.soe.ucsc.edu/hubs/VGP/genomes/$asmId > $buildDir/html/$asmId.assembly.html
fi

if [ -s ${buildDir}/trackData/assemblyGap/${asmId}.gap.bb ]; then
rm -f $buildDir/bbi/${asmId}.gap.bb
ln -s ../trackData/assemblyGap/${asmId}.gap.bb $buildDir/bbi/${asmId}.gap.bb
printf "track gap
longLabel AGP gap
shortLabel Gap (AGP defined)
visibility hide
color 0,0,0
bigDataUrl bbi/%s.gap.bb
type bigBed 4
group map
html html/%s.gap\n\n" "${asmId}" "${asmId}"
$scriptDir/asmHubGap.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/$asmId.agp.gz https://hgdownload.soe.ucsc.edu/hubs/VGP/genomes/$asmId > $buildDir/html/$asmId.gap.html
fi

if [ -s ${buildDir}/trackData/cytoBand/${asmId}.cytoBand.bb ]; then
rm -f $buildDir/bbi/${asmId}.cytoBand.bb
ln -s ../trackData/cytoBand/${asmId}.cytoBand.bb $buildDir/bbi/${asmId}.cytoBand.bb

# printf "track cytoBand
# shortLabel Chromosome Band
# longLabel Ideogram
# group map
# visibility dense
# type bigBed 4 +
# bigDataUrl bbi/%s.cytoBandIdeo.bb
# html html/%s.cytoBand\n\n" "${asmId}" "${asmId}"

# only the ideoGram is needed, not the track

printf "track cytoBandIdeo
shortLabel Chromosome Band (Ideogram)
longLabel Ideogram for Orientation
group map
visibility dense
type bigBed 4 +
bigDataUrl bbi/%s.cytoBand.bb\n\n" "${asmId}"

# $scriptDir/asmHubCytoBand.pl $asmId $buildDir/html/$asmId.names.tab $buildDir > $buildDir/html/$asmId.cytoBand.html
fi

if [ -s ${buildDir}/trackData/gc5Base/${asmId}.gc5Base.bw ]; then
rm -f $buildDir/bbi/${asmId}.gc5Base.bw
ln -s ../trackData/gc5Base/${asmId}.gc5Base.bw $buildDir/bbi/${asmId}.gc5Base.bw
printf "track gc5Base
shortLabel GC Percent
longLabel GC Percent in 5-Base Windows
group map
visibility dense
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

# see if there are gapOverlap or tandemDup bb files
export gapOverlapCount=0
export tanDupCount=0
if [ -s $buildDir/trackData/gapOverlap/${asmId}.gapOverlap.bb ]; then
  gapOverlapCount=`zcat $buildDir/trackData/gapOverlap/${asmId}.gapOverlap.bed.gz | wc -l`
  rm -f $buildDir/bbi/${asmId}.gapOverlap.bb
  ln -s ../trackData/gapOverlap/${asmId}.gapOverlap.bb $buildDir/bbi/${asmId}.gapOverlap.bb
fi
if [ -s $buildDir/trackData/tandemDups/${asmId}.tandemDups.bb ]; then
  tanDupCount=`zcat $buildDir/trackData/tandemDups/${asmId}.tandemDups.bed.gz | wc -l`
  rm -f $buildDir/bbi/${asmId}.tandemDups.bb
  ln -s ../trackData/tandemDups/${asmId}.tandemDups.bb $buildDir/bbi/${asmId}.tandemDups.bb
fi

if [ "${gapOverlapCount}" -gt 0 -o "${tanDupCount}" -gt 0 ]; then

  if [ -z ${not_tanDups+x} ]; then

  printf "track tanDups
shortLabel Tandem Dups
longLabel Paired identical sequences
compositeTrack on
visibility hide
type bigBed 12
group map
html html/%s.tanDups\n\n" "${asmId}"

  if [ "${gapOverlapCount}" -gt 0 ]; then
    printf "    track gapOverlap
    parent tanDups on
    shortLabel Gap Overlaps
    longLabel Paired exactly identical sequence on each side of a gap
    bigDataUrl bbi/%s.gapOverlap.bb
    type bigBed 12\n\n" "${asmId}"
  fi

  if [ "${tanDupCount}" -gt 0 ]; then
    printf "    track tandemDups
    parent tanDups on
    shortLabel Tandem Dups
    longLabel Paired exactly identical sequence survey over entire genome assembly
    bigDataUrl bbi/%s.tandemDups.bb
    type bigBed 12\n\n" "${asmId}"
  fi

  $scriptDir/asmHubTanDups.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData > $buildDir/html/$asmId.tanDups.html

  else
    printf "# skipping the tanDups track\n" 1>&2
  fi	#	the else clause of: if [ -z ${not_tanDups+x} ]
fi	#	if [ "${gapOverlapCount}" -gt 0 -o "${tanDupCount}" -gt 0 ]

# see if there are repeatMasker bb files
export rmskCount=`(ls $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.*.bb 2> /dev/null | wc -l) || true`
export newRmsk=`(ls $buildDir/trackData/repeatMasker/${asmId}.rmsk.align.bb $buildDir/trackData/repeatMasker/${asmId}.rmsk.bb 2> /dev/null | wc -l) || true`

if [ "${newRmsk}" -gt 0 -o "${rmskCount}" -gt 0 ]; then

if [ ! -s "$buildDir/trackData/repeatMasker/$asmId.sorted.fa.out.gz" ]; then
  printf "ERROR: can not find trackData/repeatMasker/$asmId.sorted.fa.out.gz\n" 1>&2
  exit 255
fi

# see if there are actually rmsk items in the track, this has to be > 3
export rmskItemCount=`zcat $buildDir/trackData/repeatMasker/$asmId.sorted.fa.out.gz | head | wc -l`

# clean up garbage from previous errors here
if [ "${rmskItemCount}" -lt 4 ]; then
  rm -f $buildDir/$asmId.repeatMasker.out.gz
  rm -f "$buildDir/${asmId}.repeatMasker.version.txt"
  rm -f $buildDir/bbi/${asmId}.rmsk.align.bb
  rm -f $buildDir/bbi/${asmId}.rmsk.bb
  rm -f $buildDir/${asmId}.fa.align.tsv.gz
  rm -f $buildDir/${asmId}.fa.join.tsv.gz
  rm -f $buildDir/${asmId}.rmsk.customLib.fa.gz
else

rm -f $buildDir/$asmId.repeatMasker.out.gz
ln -s trackData/repeatMasker/$asmId.sorted.fa.out.gz $buildDir/$asmId.repeatMasker.out.gz
if [ -s "$buildDir/trackData/repeatMasker/versionInfo.txt" ]; then
   rm -f "$buildDir/${asmId}.repeatMasker.version.txt"
   ln -s trackData/repeatMasker/versionInfo.txt "$buildDir/${asmId}.repeatMasker.version.txt"
fi
if [ -s "$buildDir/trackData/repeatModeler/${asmId}-families.fa" ]; then
   rm -f "$buildDir/${asmId}.rmsk.customLib.fa.gz"
   cp -p "$buildDir/trackData/repeatModeler/${asmId}-families.fa" "$buildDir/${asmId}.rmsk.customLib.fa"
   gzip "$buildDir/${asmId}.rmsk.customLib.fa"
fi

if [ "${newRmsk}" -gt 0 ]; then
  rm -f $buildDir/bbi/${asmId}.rmsk.align.bb
  rm -f $buildDir/bbi/${asmId}.rmsk.bb
  rm -f $buildDir/${asmId}.fa.align.tsv.gz
  rm -f $buildDir/${asmId}.fa.join.tsv.gz
  if [ -s "$buildDir/trackData/repeatMasker/${asmId}.rmsk.align.bb" ]; then
    ln -s ../trackData/repeatMasker/${asmId}.rmsk.align.bb $buildDir/bbi/${asmId}.rmsk.align.bb
    ln -s trackData/repeatMasker/${asmId}.fa.align.tsv.gz $buildDir/${asmId}.fa.align.tsv.gz
  fi
  ln -s ../trackData/repeatMasker/${asmId}.rmsk.bb $buildDir/bbi/${asmId}.rmsk.bb
  ln -s trackData/repeatMasker/${asmId}.sorted.fa.join.tsv.gz $buildDir/${asmId}.fa.join.tsv.gz

printf "track repeatMasker
shortLabel RepeatMasker
longLabel RepeatMasker Repetitive Elements
type bigRmsk 9 +
visibility pack
group varRep
bigDataUrl bbi/%s.rmsk.bb\n" "${asmId}"
if [ -s "$buildDir/bbi/${asmId}.rmsk.align.bb" ]; then
  printf "xrefDataUrl bbi/%s.rmsk.align.bb\n" "${asmId}"
fi
printf "maxWindowToDraw 5000000\n"
export rmskClassProfile="$buildDir/trackData/repeatMasker/$asmId.rmsk.class.profile.txt"
if [ -s "${rmskClassProfile}" ]; then
  printf "html html/%s.repeatMasker\n\n" "${asmId}"
  $scriptDir/asmHubRmskJoinAlign.pl $asmId $buildDir > $buildDir/html/$asmId.repeatMasker.html
else
  printf "\n"
fi

else	#	else clause of if [ "${newRmsk}" -gt 0 ]

printf "track repeatMasker
compositeTrack on
shortLabel RepeatMasker
longLabel Repeating Elements by RepeatMasker
group varRep
visibility dense
type bigBed 6 +
colorByStrand 50,50,150 150,50,50
maxWindowToDraw 10000000
spectrum on
html html/%s.repeatMasker\n\n" "${asmId}"
$scriptDir/asmHubRmsk.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData/repeatMasker/$asmId.rmsk.class.profile.txt > $buildDir/html/$asmId.repeatMasker.html


if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.SINE.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.SINE.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.SINE.bb $buildDir/bbi/${asmId}.rmsk.SINE.bb
printf "    track repeatMaskerSINE
    parent repeatMasker
    shortLabel SINE
    longLabel SINE Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 1
    bigDataUrl bbi/%s.rmsk.SINE.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.LINE.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.LINE.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.LINE.bb $buildDir/bbi/${asmId}.rmsk.LINE.bb
printf "    track repeatMaskerLINE
    parent repeatMasker
    shortLabel LINE
    longLabel LINE Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 2
    bigDataUrl bbi/%s.rmsk.LINE.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.LTR.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.LTR.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.LTR.bb $buildDir/bbi/${asmId}.rmsk.LTR.bb
printf "    track repeatMaskerLTR
    parent repeatMasker
    shortLabel LTR
    longLabel LTR Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 3
    bigDataUrl bbi/%s.rmsk.LTR.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.DNA.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.DNA.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.DNA.bb $buildDir/bbi/${asmId}.rmsk.DNA.bb
printf "    track repeatMaskerDNA
    parent repeatMasker
    shortLabel DNA
    longLabel DNA Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 4
    bigDataUrl bbi/%s.rmsk.DNA.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.Simple.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.Simple.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.Simple.bb $buildDir/bbi/${asmId}.rmsk.Simple.bb
printf "    track repeatMaskerSimple
    parent repeatMasker
    shortLabel Simple
    longLabel Simple Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 5
    bigDataUrl bbi/%s.rmsk.Simple.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.Low_complexity.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.Low_complexity.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.Low_complexity.bb $buildDir/bbi/${asmId}.rmsk.Low_complexity.bb
printf "    track repeatMaskerLowComplexity
    parent repeatMasker
    shortLabel Low Complexity
    longLabel Low Complexity Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 6
    bigDataUrl bbi/%s.rmsk.Low_complexity.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.Satellite.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.Satellite.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.Satellite.bb $buildDir/bbi/${asmId}.rmsk.Satellite.bb
printf "    track repeatMaskerSatellite
    parent repeatMasker
    shortLabel Satellite
    longLabel Satellite Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 7
    bigDataUrl bbi/%s.rmsk.Satellite.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.RNA.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.RNA.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.RNA.bb $buildDir/bbi/${asmId}.rmsk.RNA.bb
printf "    track repeatMaskerRNA
    parent repeatMasker
    shortLabel RNA
    longLabel RNA Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 8
    bigDataUrl bbi/%s.rmsk.RNA.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/repeatMasker/bbi/${asmId}.rmsk.Other.bb ]; then
rm -f $buildDir/bbi/${asmId}.rmsk.Other.bb
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.Other.bb $buildDir/bbi/${asmId}.rmsk.Other.bb
printf "    track repeatMaskerOther
    parent repeatMasker
    shortLabel Other
    longLabel Other Repeating Elements by RepeatMasker
    type bigBed 6 +
    priority 9
    bigDataUrl bbi/%s.rmsk.Other.bb\n\n" "${asmId}"
fi

fi	#	else clause of if [ "${newRmsk}" -gt 0 ]; then
fi	#	else clause of if [ "${rmskItemCount}" -lt 4 ]
fi      #       if [ "${newRmsk}" -gt 0 -o "${rmskCount}" -gt 0 ]; then

# see if there are repeatModeler bb files
export rModelCount=`(ls $buildDir/trackData/repeatModeler/bbi/${asmId}.rmsk.*.bb 2> /dev/null | wc -l) || true`
export newRmodel=`(ls $buildDir/trackData/repeatModeler/${asmId}.rmsk.align.bb $buildDir/trackData/repeatModeler/${asmId}.rmsk.bb 2> /dev/null | wc -l) || true`

if [ "${newRmodel}" -gt 0 -o "${rModelCount}" -gt 0 ]; then

if [ ! -s "$buildDir/trackData/repeatModeler/$asmId.sorted.fa.out.gz" ]; then
  printf "ERROR: can not find trackData/repeatModeler/$asmId.sorted.fa.out.gz\n" 1>&2
  exit 255
fi

# see if there are actually rmsk items in the track, this has to be > 3
export rModelItemCount=`zcat $buildDir/trackData/repeatModeler/$asmId.sorted.fa.out.gz | head | wc -l`

# clean up garbage from previous errors here
if [ "${rModelItemCount}" -lt 4 ]; then
  rm -f $buildDir/$asmId.repeatModeler.out.gz
  rm -f "$buildDir/${asmId}.repeatModeler.version.txt"
  rm -f $buildDir/bbi/${asmId}.rModel.align.bb
  rm -f $buildDir/bbi/${asmId}.rModel.bb
  rm -f $buildDir/${asmId}.fa.rModel.align.tsv.gz
  rm -f $buildDir/${asmId}.fa.rModel.join.tsv.gz
  rm -f $buildDir/${asmId}.rModel.customLib.fa.gz
else

rm -f $buildDir/$asmId.repeatModeler.out.gz
ln -s trackData/repeatModeler/$asmId.sorted.fa.out.gz $buildDir/$asmId.repeatModeler.out.gz
if [ -s "$buildDir/trackData/repeatModeler/versionInfo.txt" ]; then
   rm -f "$buildDir/${asmId}.repeatModeler.version.txt"
   ln -s trackData/repeatModeler/versionInfo.txt "$buildDir/${asmId}.repeatModeler.version.txt"
fi
if [ -s "$buildDir/trackData/repeatModeler/${asmId}-families.fa" ]; then
   rm -f "$buildDir/${asmId}.rmsk.customLib.fa.gz"
   cp -p "$buildDir/trackData/repeatModeler/${asmId}-families.fa" "$buildDir/${asmId}.rmsk.customLib.fa"
   gzip "$buildDir/${asmId}.rmsk.customLib.fa"
fi

if [ "${newRmodel}" -gt 0 ]; then
  rm -f $buildDir/bbi/${asmId}.rModel.align.bb
  rm -f $buildDir/bbi/${asmId}.rModel.bb
  rm -f $buildDir/${asmId}.fa.rModel.align.tsv.gz
  rm -f $buildDir/${asmId}.fa.rModel.join.tsv.gz
  if [ -s "$buildDir/trackData/repeatModeler/${asmId}.rmsk.align.bb" ]; then
    ln -s ../trackData/repeatModeler/${asmId}.rmsk.align.bb $buildDir/bbi/${asmId}.rModel.align.bb
    ln -s trackData/repeatModeler/${asmId}.fa.align.tsv.gz $buildDir/${asmId}.fa.rModel.align.tsv.gz
  fi
  ln -s ../trackData/repeatModeler/${asmId}.rmsk.bb $buildDir/bbi/${asmId}.rModel.bb
  ln -s trackData/repeatModeler/${asmId}.sorted.fa.join.tsv.gz $buildDir/${asmId}.fa.rModel.join.tsv.gz

printf "track repeatModeler
shortLabel RepeatModeler
longLabel RepeatModeler Repetitive Elements
type bigRmsk 9 +
visibility hide
group varRep
bigDataUrl bbi/%s.rModel.bb\n" "${asmId}"
if [ -s "$buildDir/bbi/${asmId}.rModel.align.bb" ]; then
  printf "xrefDataUrl bbi/%s.rModel.align.bb\n" "${asmId}"
fi
printf "maxWindowToDraw 5000000\n"
export rModelClassProfile="$buildDir/trackData/repeatModeler/$asmId.rmsk.class.profile.txt"
if [ -s "${rModelClassProfile}" ]; then
  printf "html html/%s.repeatModeler\n\n" "${asmId}"
  $scriptDir/asmHubRmodelJoinAlign.pl $asmId $buildDir > $buildDir/html/$asmId.repeatModeler.html
else
  printf "\n"
fi

else	#	else clause of if [ "${newRmodel}" -gt 0 ]

  printf "ERROR: expected new version of rmsk files for RepeatModeler not found\n" 1>&2
  exit 255

fi	#	else clause of if [ "${newRmodel}" -gt 0 ]; then
fi	#	else clause of if [ "${rModelItemCount}" -lt 4 ]
fi      #       if [ "${newRmodel}" -gt 0 -o "${rModelCount}" -gt 0 ]; then

if [ -s ${buildDir}/trackData/simpleRepeat/simpleRepeat.bb ]; then
rm -f $buildDir/bbi/${asmId}.simpleRepeat.bb
ln -s ../trackData/simpleRepeat/simpleRepeat.bb $buildDir/bbi/${asmId}.simpleRepeat.bb
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

### assume there is no ncbiRefSeq track
### when there is, this will eliminate the ncbiGene track
### and it figures into setting the visibility of the augustus gene track
export haveNcbiRefSeq="no"

###################################################################
# ncbiRefSeq composite track
if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeq.bb ]; then
rm -f $buildDir/bbi/${asmId}.ncbiRefSeq.bb
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeq.ix
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeq.ixx
#  also remove all these other ones, they may not exist and their if 'exist'
#    statements below will not remove them if they were there before
rm -f $buildDir/bbi/${asmId}.ncbiRefSeqCurated.bb
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqCurated.ix
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqCurated.ixx
rm -f $buildDir/bbi/${asmId}.ncbiRefSeqPredicted.bb
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqPredicted.ix
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqPredicted.ixx
rm -f $buildDir/bbi/${asmId}.ncbiRefSeqOther.bb
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqOther.ix
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqOther.ixx
rm -f $buildDir/bbi/${asmId}.bigPsl.bb
rm -f $buildDir/bbi/${asmId}.ncbiRefSeqSelectCurated.bb
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqSelectCurated.ix
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqSelectCurated.ixx
ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeq.bb $buildDir/bbi/${asmId}.ncbiRefSeq.bb
ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeq.ix $buildDir/ixIxx/${asmId}.ncbiRefSeq.ix
ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeq.ixx $buildDir/ixIxx/${asmId}.ncbiRefSeq.ixx
if [ -s ${buildDir}/trackData/ncbiRefSeq/${asmId}*.ncbiRefSeq.gtf.gz ]; then
    mkdir -p $buildDir/genes
    rm -f ${buildDir}/genes/${asmId}.ncbiRefSeq.gtf.gz
    gtfFile=`ls ${buildDir}/trackData/ncbiRefSeq/${asmId}*.ncbiRefSeq.gtf.gz|tail -1|sed -e 's#.*/##;'`
    ln -s ../trackData/ncbiRefSeq/${gtfFile} ${buildDir}/genes/${asmId}.ncbiRefSeq.gtf.gz
fi

  export dataVersion="html/ncbiRefSeqVersion.txt"
  if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeqVersion.txt ]; then
   dataVersion=`cat ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeqVersion.txt`
  fi

  printf "track refSeqComposite
compositeTrack on
shortLabel NCBI RefSeq
longLabel RefSeq gene predictions from NCBI
group genes
visibility pack
type bigBed
dragAndDrop subTracks
allButtonPair on
dataVersion $dataVersion
html html/%s.refSeqComposite
priority 2

        track ncbiRefSeq
        parent refSeqComposite on
        color 12,12,120
        shortLabel RefSeq All
        type bigGenePred
        urls name2=\"https://www.ncbi.nlm.nih.gov/gene/?term=\$\$\" geneName=\"https://www.ncbi.nlm.nih.gov/gene/\$\$\" geneName2=\"https://www.ncbi.nlm.nih.gov/nuccore/\$\$\"
        labelFields name2,geneName,geneName2
        defaultLabelFields name2
        searchIndex name
        searchTrix ixIxx/%s.ncbiRefSeq.ix
        bigDataUrl bbi/%s.ncbiRefSeq.bb
        longLabel NCBI RefSeq genes, curated and predicted sets (NM_*, XM_*, NR_*, XR_*, NP_* or YP_*)
        idXref ncbiRefSeqLink mrnaAcc name
        baseColorUseCds given
        baseColorDefault genomicCodons
        priority 1\n\n" "${asmId}" "${asmId}" "${asmId}"

  if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeqCurated.bb ]; then
    rm -f $buildDir/bbi/${asmId}.ncbiRefSeqCurated.bb
    rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqCurated.ix
    rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqCurated.ixx
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqCurated.bb $buildDir/bbi/${asmId}.ncbiRefSeqCurated.bb
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqCurated.ix $buildDir/ixIxx/${asmId}.ncbiRefSeqCurated.ix
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqCurated.ixx $buildDir/ixIxx/${asmId}.ncbiRefSeqCurated.ixx

    printf "        track ncbiRefSeqCurated
        color 12,12,120
        parent refSeqComposite on
        shortLabel RefSeq Curated
        longLabel NCBI RefSeq genes, curated subset (NM_*, NR_*, NP_* or YP_*)
        type bigGenePred
        urls name2=\"https://www.ncbi.nlm.nih.gov/gene/?term=\$\$\" geneName=\"https://www.ncbi.nlm.nih.gov/gene/\$\$\" geneName2=\"https://www.ncbi.nlm.nih.gov/nuccore/\$\$\"
        labelFields name2,geneName,geneName2
        defaultLabelFields name2
        searchIndex name
        searchTrix ixIxx/%s.ncbiRefSeqCurated.ix
        idXref ncbiRefSeqLink mrnaAcc name
        bigDataUrl bbi/%s.ncbiRefSeqCurated.bb
        baseColorUseCds given
        baseColorDefault genomicCodons
        priority 2\n\n" "${asmId}" "${asmId}"
  fi

  if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeqPredicted.bb ]; then
    rm -f $buildDir/bbi/${asmId}.ncbiRefSeqPredicted.bb
    rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqPredicted.ix
    rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqPredicted.ixx
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqPredicted.bb $buildDir/bbi/${asmId}.ncbiRefSeqPredicted.bb
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqPredicted.ix $buildDir/ixIxx/${asmId}.ncbiRefSeqPredicted.ix
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqPredicted.ixx $buildDir/ixIxx/${asmId}.ncbiRefSeqPredicted.ixx

    printf "        track ncbiRefSeqPredicted
        color 12,12,120
        parent refSeqComposite on
        shortLabel RefSeq Predicted
        longLabel NCBI RefSeq genes, predicted subset (XM_* or XR_*)
        type bigGenePred
        urls name2=\"https://www.ncbi.nlm.nih.gov/gene/?term=\$\$\" geneName=\"https://www.ncbi.nlm.nih.gov/gene/\$\$\" geneName2=\"https://www.ncbi.nlm.nih.gov/nuccore/\$\$\"
        labelFields name2,geneName,geneName2
        defaultLabelFields name2
        searchIndex name
        searchTrix ixIxx/%s.ncbiRefSeqPredicted.ix
        idXref ncbiRefSeqLink mrnaAcc name
        bigDataUrl bbi/%s.ncbiRefSeqPredicted.bb
        baseColorUseCds given
        baseColorDefault genomicCodons
        priority 3\n\n" "${asmId}" "${asmId}"
  fi

  if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeqOther.bb ]; then
    rm -f $buildDir/bbi/${asmId}.ncbiRefSeqOther.bb
    rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqOther.ix
    rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqOther.ixx
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqOther.bb $buildDir/bbi/${asmId}.ncbiRefSeqOther.bb
rm -f $buildDir/ixIxx/${asmId}.xenoRefGene.ix
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqOther.ix $buildDir/ixIxx/${asmId}.ncbiRefSeqOther.ix
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqOther.ixx $buildDir/ixIxx/${asmId}.ncbiRefSeqOther.ixx

    printf "        track ncbiRefSeqOther
        color 32,32,32
        parent refSeqComposite on
        shortLabel RefSeq Other
        longLabel NCBI RefSeq other annotations (not NM_*, NR_*, XM_*, XR_*, NP_* or YP_*)
        priority 4
        searchIndex name
        searchTrix ixIxx/%s.ncbiRefSeqOther.ix
        bigDataUrl bbi/%s.ncbiRefSeqOther.bb
        type bigBed 12 +
        labelFields name
        skipEmptyFields on
        urls GeneID=\"https://www.ncbi.nlm.nih.gov/gene/\$\$\" MIM=\"https://www.ncbi.nlm.nih.gov/omim/\$\$\" HGNC=\"http://www.genenames.org/cgi-bin/gene_symbol_report?hgnc_id=\$\$\" FlyBase=\"http://flybase.org/reports/\$\$\" WormBase=\"http://www.wormbase.org/db/gene/gene?name=\$\$\" RGD=\"https://rgd.mcw.edu/rgdweb/search/search.html?term=\$\$\" SGD=\"https://www.yeastgenome.org/locus/\$\$\" miRBase=\"http://www.mirbase.org/cgi-bin/mirna_entry.pl?acc=\$\$\" ZFIN=\"https://zfin.org/\$\$\" MGI=\"http://www.informatics.jax.org/marker/\$\$\"\n\n" "${asmId}" "${asmId}"

  fi

  if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.bigPsl.bb ]; then
    rm -f $buildDir/bbi/${asmId}.bigPsl.bb
    ln -s ../trackData/ncbiRefSeq/$asmId.bigPsl.bb $buildDir/bbi/${asmId}.bigPsl.bb

    printf "        track ncbiRefSeqPsl
        priority 5
        parent refSeqComposite off
        shortLabel RefSeq Alignments
        longLabel RefSeq Alignments of RNAs
        type bigPsl
        searchIndex name
        bigDataUrl bbi/%s.bigPsl.bb
        indelDoubleInsert on
        indelQueryInsert on
        showDiffBasesAllScales .
        showDiffBasesMaxZoom 10000.0
        showCdsMaxZoom 10000.0
        showCdsAllScales .
        baseColorDefault diffCodons
        pslSequence no
        baseColorUseSequence lfExtra
        baseColorUseCds table given
        color 0,0,0\n\n" "${asmId}"
  fi

  if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeqSelectCurated.bb ]; then
    rm -f $buildDir/bbi/${asmId}.ncbiRefSeqSelectCurated.bb
    rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqSelectCurated.ix
    rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeqSelectCurated.ixx
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqSelectCurated.bb $buildDir/bbi/${asmId}.ncbiRefSeqSelectCurated.bb
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqSelectCurated.ix $buildDir/ixIxx/${asmId}.ncbiRefSeqSelectCurated.ix
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqSelectCurated.ixx $buildDir/ixIxx/${asmId}.ncbiRefSeqSelectCurated.ixx

    printf "        track ncbiRefSeqSelect
        parent refSeqComposite off
        priority 7
        type bigGenePred
        shortLabel RefSeq Select
        longLabel NCBI RefSeq/MANE Select: one representative transcript per protein-coding gene
        idXref ncbiRefSeqLink mrnaAcc name
        color 20,20,160
        labelFields name,geneName,geneName2
        searchIndex name
        searchTrix ixIxx/%s.ncbiRefSeqSelectCurated.ix
        bigDataUrl bbi/%s.ncbiRefSeqSelectCurated.bb
        baseColorUseCds given
        baseColorDefault genomicCodons\n\n" "${asmId}" "${asmId}"
  fi

  if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeqVersion.txt ]; then
    rm -f $buildDir/html/$asmId.ncbiRefSeqVersion.txt
    ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeqVersion.txt $buildDir/html/
  fi

  $scriptDir/asmHubNcbiRefSeq.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData > $buildDir/html/$asmId.refSeqComposite.html

haveNcbiRefSeq="yes"
fi	# ncbiRefSeq composite track

### assume there is no ncbiGene track
### it figures into setting the visibility of the augustus gene track
export haveNcbiGene="no"

###################################################################
## setup ncbiGene only if ncbiRefSeq is not present
if [ "${haveNcbiRefSeq}" = "no" ]; then


# may or may not have a searchTrix for ncbiGene, assume none
searchTrix=""
# check to see if there is a index for ncbiGene
if [ -s ${buildDir}/trackData/ncbiGene/$asmId.ncbiGene.ix ]; then
  searchTrix="
searchTrix ixIxx/$asmId.ncbiGene.ix"
fi


if [ -s ${buildDir}/trackData/ncbiGene/$asmId.ncbiGene.bb ]; then
rm -f $buildDir/bbi/${asmId}.ncbiGene.bb
rm -f $buildDir/ixIxx/${asmId}.ncbiGene.ix
rm -f $buildDir/ixIxx/${asmId}.ncbiGene.ixx
rm -f ${buildDir}/genes/${asmId}.ncbiGene.gtf.gz
export longLabel="Gene models submitted to NCBI"
export shortLabel="Gene models"
if [ "$asmType" = "refseq" ]; then
  longLabel="RefSeq gene predictions from NCBI"
  shortLabel="NCBI RefSeq"
fi
if [ -s ${buildDir}/trackData/ncbiGene/${asmId}.ncbiGene.gtf.gz ]; then
  mkdir -p $buildDir/genes
  ln -s ../trackData/ncbiGene/${asmId}.ncbiGene.gtf.gz $buildDir/genes/${asmId}.ncbiGene.gtf.gz
fi
ln -s ../trackData/ncbiGene/$asmId.ncbiGene.bb $buildDir/bbi/${asmId}.ncbiGene.bb
if [ -s  $buildDir/ixIxx/${asmId}.ncbiGene.ix ]; then
  ln -s ../trackData/ncbiGene/$asmId.ncbiGene.ix $buildDir/ixIxx/${asmId}.ncbiGene.ix
  ln -s ../trackData/ncbiGene/$asmId.ncbiGene.ixx $buildDir/ixIxx/${asmId}.ncbiGene.ixx
fi
  printf "track ncbiGene
longLabel $longLabel
shortLabel $shortLabel
visibility pack
color 0,80,150
altColor 150,80,0
colorByStrand 0,80,150 150,80,0
bigDataUrl bbi/%s.ncbiGene.bb
type bigGenePred
urls name2=\"https://www.ncbi.nlm.nih.gov/gene/?term=\$\$\" geneName=\"https://www.ncbi.nlm.nih.gov/gene/\$\$\" geneName2=\"https://www.ncbi.nlm.nih.gov/nuccore/\$\$\"
html html/%s.ncbiGene
searchIndex name%s
urlLabel Entrez gene:
labelFields geneName,geneName2
defaultLabelFields geneName2
group genes\n\n" "${asmId}" "${asmId}" "${searchTrix}"

  $scriptDir/asmHubNcbiGene.pl $asmId $ncbiAsmId $buildDir/html/$asmId.names.tab $buildDir/trackData > $buildDir/html/$asmId.ncbiGene.html

haveNcbiGene="yes"
fi	#	if [ -s ${buildDir}/trackData/ncbiGene/$asmId.ncbiGene.bb ]
fi	#	if [ "${haveNcbiRefSeq}" = "no" ]
###################################################################

###################################################################
# CpG Islands composite
export cpgVis="off"
# if there is no unmasked track, then set cpgVis to pack
if [ ! -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb ]; then
  cpgVis="on"
fi
if [ -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb -o -s ${buildDir}/trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.cpgIslandExtUnmasked.bb ${buildDir}/bbi/${asmId}.cpgIslandExt.bb

printf "track cpgIslands
compositeTrack on
shortLabel CpG Islands
longLabel CpG Islands (Islands < 300 Bases are Light Green)
group regulation
visibility dense
type bigBed 4 +
html html/%s.cpgIslands\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ]; then
ln -s ../trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ${buildDir}/bbi/${asmId}.cpgIslandExt.bb
printf "    track cpgIslandExt
    parent cpgIslands %s
    shortLabel CpG Islands
    longLabel CpG Islands (Islands < 300 Bases are Light Green)
    type bigBed 4 +
    priority 1
    bigDataUrl bbi/%s.cpgIslandExt.bb\n\n" "${cpgVis}" "${asmId}"
fi

if [ -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb ]; then
ln -s ../trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb ${buildDir}/bbi/${asmId}.cpgIslandExtUnmasked.bb
printf "    track cpgIslandExtUnmasked
    parent cpgIslands on
    shortLabel Unmasked CpG
    longLabel CpG Islands on All Sequence (Islands < 300 Bases are Light Green)
    type bigBed 4 +
    priority 2
    bigDataUrl bbi/%s.cpgIslandExtUnmasked.bb\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb -o -s ${buildDir}/trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ]; then
  $scriptDir/asmHubCpG.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.cpgIslands.html
fi

###################################################################
# windowMasker
if [ -s ${buildDir}/trackData/windowMasker/${asmId}.windowMasker.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.windowMasker.bb
ln -s ../trackData/windowMasker/${asmId}.windowMasker.bb ${buildDir}/bbi/${asmId}.windowMasker.bb

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
ln -s ../trackData/allGaps/${asmId}.allGaps.bb ${buildDir}/bbi/${asmId}.allGaps.bb

printf "track allGaps
shortLabel All Gaps
longLabel All gaps of unknown nucleotides (N's), including AGP annotated gaps
group map
visibility hide
type bigBed 3
bigDataUrl bbi/%s.allGaps.bb
html html/%s.allGaps\n\n" "${asmId}" "${asmId}"
$scriptDir/asmHubAllGaps.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/$asmId.agp.gz https://hgdownload.soe.ucsc.edu/hubs/VGP/genomes/$asmId $buildDir/bbi/$asmId > $buildDir/html/$asmId.allGaps.html
fi

###################################################################
# augustus genes
if [ -z ${not_augustus+x} ]; then

if [ -s ${buildDir}/trackData/augustus/${asmId}.augustus.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.augustus.bb
rm -f ${buildDir}/genes/${asmId}.augustus.gtf.gz
ln -s ../trackData/augustus/${asmId}.augustus.bb ${buildDir}/bbi/${asmId}.augustus.bb
if [ -s ${buildDir}/trackData/augustus/${asmId}.augustus.gtf.gz ]; then
    mkdir -p $buildDir/genes
    ln -s ../trackData/augustus/${asmId}.augustus.gtf.gz ${buildDir}/genes/${asmId}.augustus.gtf.gz
fi

export augustusVis="dense"

if [ "${haveNcbiGene}" = "no" -a "${haveNcbiRefSeq}" = "no" ]; then
   augustusVis="pack"
fi
printf "track augustus
shortLabel Augustus
longLabel Augustus Gene Predictions
group genes
visibility %s
color 180,0,0
type bigGenePred
bigDataUrl bbi/%s.augustus.bb
html html/%s.augustus\n\n" "${augustusVis}" "${asmId}" "${asmId}"
$scriptDir/asmHubAugustusGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.augustus.html
fi

else
  printf "# skipping the augustus track\n" 1>&2
fi	#	the else clause of: if [ -z ${not_augustus+x} ]

###################################################################
# xenoRefGene genes
if [ -s ${buildDir}/trackData/xenoRefGene/${asmId}.xenoRefGene.bb ]; then
rm -f $buildDir/ixIxx/${asmId}.xenoRefGene.ix
rm -f $buildDir/ixIxx/${asmId}.xenoRefGene.ixx
rm -f $buildDir/bbi/${asmId}.xenoRefGene.bb
rm -f $buildDir/genes/${asmId}.xenoRefGene.gtf.gz
  if [ -s ${buildDir}/trackData/xenoRefGene/${asmId}.xenoRefGene.gtf.gz ]; then
    mkdir -p $buildDir/genes
    ln -s ../trackData/xenoRefGene/${asmId}.xenoRefGene.gtf.gz ${buildDir}/genes/${asmId}.xenoRefGene.gtf.gz
  fi
ln -s ../trackData/xenoRefGene/${asmId}.xenoRefGene.bb ${buildDir}/bbi/${asmId}.xenoRefGene.bb
ln -s ../trackData/xenoRefGene/$asmId.xenoRefGene.ix $buildDir/ixIxx/${asmId}.xenoRefGene.ix
ln -s ../trackData/xenoRefGene/$asmId.xenoRefGene.ixx $buildDir/ixIxx/${asmId}.xenoRefGene.ixx

printf "track xenoRefGene
shortLabel RefSeq mRNAs
longLabel RefSeq mRNAs mapped to this assembly
group rna
visibility pack
color 180,0,0
type bigGenePred
bigDataUrl bbi/%s.xenoRefGene.bb
url https://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database:
labelFields name,geneName,geneName2
defaultLabelFields geneName
searchIndex name
searchTrix ixIxx/%s.xenoRefGene.ix
html html/%s.xenoRefGene\n\n" "${asmId}" "${asmId}" "${asmId}"
$scriptDir/asmHubXenoRefGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData > $buildDir/html/$asmId.xenoRefGene.html
fi

###################################################################
# Ensembl genes
if [ -s ${buildDir}/trackData/ensGene/bbi/${asmId}.ensGene.bb ]; then
export oldStyle=`bigBedInfo ${buildDir}/trackData/ensGene/bbi/${asmId}.ensGene.bb | grep -c "fieldCount: 12"`
rm -f ${buildDir}/bbi/${asmId}.ensGene.bb
ln -s ../trackData/ensGene/bbi/${asmId}.ensGene.bb ${buildDir}/bbi/${asmId}.ensGene.bb
rm -f ${buildDir}/ixIxx/${asmId}.ensGene.ix
rm -f ${buildDir}/ixIxx/${asmId}.ensGene.ixx
rm -f ${buildDir}/genes/${asmId}.ensGene.*.gtf.gz
export gtfGz=`ls ${buildDir}/trackData/ensGene/${asmId}.ensGene.*.gtf.gz`
if [ -s "${gtfGz}" ]; then
    mkdir -p ${buildDir}/genes
    bName=`basename "${gtfGz}"`
   ln -s ../trackData/ensGene/${bName} ${buildDir}/genes/${bName}
fi

### if we had more than one index, but no, we are using ixIxx files
# export indexList=`bigBedInfo -extraIndex ${buildDir}/bbi/${asmId}.ensGene.bb | grep -w field | grep -w with | awk '{print $1}' | xargs echo | tr ' ' ','`

export indexList="name"

# optional ix/ixx files, this string used in the trackDb stanza below
export searchTrix=""
if [ -s "${buildDir}/trackData/ensGene/${asmId}.ensGene.ix" ]; then
  ln -s ../trackData/ensGene/${asmId}.ensGene.ix $buildDir/ixIxx/${asmId}.ensGene.ix
  ln -s ../trackData/ensGene/${asmId}.ensGene.ixx $buildDir/ixIxx/${asmId}.ensGene.ixx
      searchTrix="
searchTrix ixIxx/${asmId}.ensGene.ix"
    fi

export ensVersion="v86"

if [ -s ${buildDir}/trackData/ensGene/version.txt ]; then
  ensVersion=`cat "${buildDir}/trackData/ensGene/version.txt"`
fi


if [ ${oldStyle} -eq 1 ]; then
  printf "track ensGene
shortLabel Ensembl genes
longLabel Ensembl genes %s
group genes
priority 40
visibility pack
color 150,0,0
itemRgb on
type bigBed 12 .
bigDataUrl bbi/%s.ensGene.bb%s
searchIndex %s
baseColorUseCds given
baseColorDefault genomicCodons
html html/%s.ensGene\n\n" "${ensVersion}" "${asmId}" "${searchTrix}" "${indexList}" "${asmId}"
else
  printf "track ensGene
shortLabel Ensembl genes
longLabel Ensembl genes %s
group genes
priority 40
visibility pack
color 150,0,0
itemRgb on
type bigGenePred
bigDataUrl bbi/%s.ensGene.bb%s
searchIndex %s
labelFields name,name2
defaultLabelFields name2
baseColorUseCds given
baseColorDefault genomicCodons
labelSeparator \" \"
html html/%s.ensGene\n\n" "${ensVersion}" "${asmId}" "${searchTrix}" "${indexList}" "${asmId}"
fi

$scriptDir/asmHubEnsGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.ensGene.html "${ensVersion}"

else
  printf "# no ensGene found\n" 1>&2
fi

###################################################################
# Ensembl/ebiGene for HPRC project
if [ -d ${buildDir}/trackData/ebiGene ]; then
 export ebiGeneBb=`ls ${buildDir}/trackData/ebiGene/*.bb 2> /dev/null || true | head -1`
 if [ -s "${ebiGeneBb}" ]; then
    export ebiGeneLink=`echo $ebiGeneBb | sed -e 's#.*trackData#../trackData#;'`
    rm -f ${buildDir}/bbi/${asmId}.ebiGene.bb
    ln -s $ebiGeneLink ${buildDir}/bbi/${asmId}.ebiGene.bb
    rm -f ${buildDir}/ixIxx/${asmId}.ebiGene.ix
    rm -f ${buildDir}/ixIxx/${asmId}.ebiGene.ixx
    export ixLink=`echo $ebiGeneBb | sed -e 's#.*trackData#../trackData#; s#.bb#.ix#'`
    export ixxLink=`echo $ebiGeneBb | sed -e 's#.*trackData#../trackData#; s#.bb#.ixx#'`
    # optional ix/ixx files, this string used in the trackDb stanza below
    export searchTrix=""
    export ebiGeneIx=`ls ${buildDir}/trackData/ebiGene/*.ix 2> /dev/null || true | head -1`
    if [ -s "${ebiGeneIx}" ]; then
      ln -s $ixLink ${buildDir}/ixIxx/${asmId}.ebiGene.ix
      ln -s $ixxLink ${buildDir}/ixIxx/${asmId}.ebiGene.ixx
      searchTrix="
searchTrix ixIxx/${asmId}.ebiGene.ix"
    fi
    export ebiVersion="2022_08"

    if [ -s ${buildDir}/trackData/ebiGene/version.txt ]; then
      ebiVersion=`cat "${buildDir}/trackData/ebiGene/version.txt"`
    fi

    printf "track ebiGene
shortLabel Ensembl %s
longLabel Ensembl genes version %s
group genes
visibility pack
color 150,0,0
itemRgb on
type bigGenePred
bigDataUrl bbi/%s.ebiGene.bb%s
searchIndex name,name2
labelFields name,name2
defaultLabelFields name2
labelSeparator \" \"
html html/%s.ebiGene\n\n" "${ebiVersion}" "${ebiVersion}" "${asmId}" "${searchTrix}" "${asmId}"

$scriptDir/asmHubEbiGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.ebiGene.html "${ebiVersion}"
 fi

fi

###################################################################
# hubLinks is for mouseStrains specific hub only
export hubLinks="/hive/data/genomes/asmHubs/hubLinks"
if [ -s ${hubLinks}/${asmId}/rnaSeqData/$asmId.trackDb.txt ]; then
  printf "include rnaSeqData/%s.trackDb.txt\n\n" "${asmId}"
fi
##  for mouse strain hubs only
## turned off 2022-11-02 until these can be correctly translated
## to GenArk naming schemes
### if [ -s "${buildDir}/$asmId.bigMaf.trackDb.txt" ]; then
###   printf "include %s.bigMaf.trackDb.txt\n\n" "${asmId}"
### fi

###################################################################
# check for blat sameSpecies liftOver, then link to lift over chain file
export lo=`ls -d ${buildDir}/trackData/blat.* 2> /dev/null | wc -l`

if [ "${lo}" -gt 0 ]; then
  mkdir -p ${buildDir}/liftOver
  for loS in `ls -d ${buildDir}/trackData/blat.* 2> /dev/null`
  do
     blatDir=`basename "${loS}"`
     overChain=`(ls ${loS}/*.over.chain.gz || true) | awk -F'/' "{print \\$NF}"`
     if [ "x${overChain}y" != "xy" ]; then
       rm -f ${buildDir}/liftOver/${overChain}
     fi
     if [ -s "${buildDir}/trackData/${blatDir}/${overChain}" ]; then
       ln -s ../trackData/${blatDir}/${overChain} ${buildDir}/liftOver
     fi
  done
fi

###################################################################
# check for lastz/chain/net available

export lz=`ls -d ${buildDir}/trackData/lastz.* 2> /dev/null | wc -l`

if [ "${lz}" -gt 0 ]; then
  if [ "${lz}" -eq 1 ]; then
printf "single chainNet\n" 1>&2
    export lastzDir=`ls -d ${buildDir}/trackData/lastz.*`
    export oOrganism=`basename "${lastzDir}" | sed -e 's/lastz.//;'`
    # single chainNet here, no need for a composite track, does the symLinks too
    $scriptDir/asmHubChainNetTrackDb.sh $asmId $buildDir
    $scriptDir/asmHubChainNet.pl $asmId $ncbiAsmId $buildDir/html/$asmId.names.tab $oOrganism > $buildDir/html/$asmId.chainNet.html
  else
printf "composite chainNet\n" 1>&2
    # multiple chainNets here, create composite track, does the symLinks too
    $scriptDir/asmHubChainNetTrackDb.pl $buildDir
    $scriptDir/asmHubChainNetComposite.pl $asmId $ncbiAsmId $buildDir/html/$asmId.names.tab > $buildDir/html/$asmId.chainNet.html
  fi
fi

###################################################################
# crisprAll track

if [ -s ${buildDir}/trackData/crisprAll/crispr.bb ]; then

rm -f $buildDir/bbi/${asmId}.crisprAll.bb
rm -f $buildDir/bbi/${asmId}.crisprAllDetails.tab
ln -s ../trackData/crisprAll/crispr.bb ${buildDir}/bbi/${asmId}.crisprAll.bb
ln -s ../trackData/crisprAll/crisprDetails.tab $buildDir/bbi/${asmId}.crisprAllDetails.tab

printf "track crisprAllTargets
visibility hide
shortLabel CRISPR Targets
longLabel CRISPR/Cas9 -NGG Targets, whole genome
group genes
type bigBed 9 +
html html/%s.crisprAll
itemRgb on
mouseOverField _mouseOver
scoreLabel MIT Guide Specificity Score
bigDataUrl bbi/%s.crisprAll.bb
# details page is not using a mysql table but a tab-sep file
detailsTabUrls _offset=bbi/%s.crisprAllDetails.tab
url http://crispor.tefor.net/crispor.py?org=\$D&pos=\$S:\${&pam=NGG
urlLabel Click here to show this guide on Crispor.org, with expression oligos, validation primers and more
tableBrowser noGenome
noGenomeReason This track is too big for whole-genome Table Browser access, it would lead to a timeout in your internet browser. Please see the CRISPR Track documentation, the section \"Data Access\", for bulk-download options. Contact us if you encounter difficulties with downloading the data.
denseCoverage 0
scoreFilterMax 100
" "${asmId}" "${asmId}" "${asmId}"

$scriptDir/asmHubCrisprAll.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData > $buildDir/html/$asmId.crisprAll.html

fi	#	if [ -s ${buildDir}/trackData/crisprAll/crispr.bb ]

# TOGA track if exists
# build directory can be either TOGAvGalGal6v1 or TOGAvHg38v1

export tg=`ls -d ${buildDir}/trackData/TOGAv* 2> /dev/null | wc -l`
if [ "${tg}" -gt 0 ]; then
  rm -f $buildDir/bbi/HLTOGAannotVs*.*
  rm -f $buildDir/ixIxx/HLTOGAannotVs*.*
  tData=`ls -d $buildDir/trackData/TOGAv* | sed -e 's#.*/trackData#trackData#;'`
  fBase=`ls $buildDir/trackData/TOGAv*/HLTOGAannotVs*.bb | sed -e 's#.*/##; s/.bb//;'`
  # there is a bug in the source files that have galGal6 when they should
  # be GalGal6
  FBase=`echo $fBase | sed -e 's#galGal6#GalGal6#;'`
  ln -s ../$tData/$fBase.bb $buildDir/bbi/$FBase.bb
  ln -s ../$tData/$fBase.ix $buildDir/ixIxx/$FBase.ix
  ln -s ../$tData/$fBase.ixx $buildDir/ixIxx/$FBase.ixx
  if [ -d ${buildDir}/trackData/TOGAvHg38v1 ]; then
printf "track HLTOGAannotvHg38v1
bigDataUrl bbi/HLTOGAannotVsHg38v1.bb
shortLabel TOGA vs. hg38
longLabel TOGA annotations using human/hg38 as reference
group genes
visibility pack
itemRgb on
type bigBed 12
searchIndex name
searchTrix  ixIxx/HLTOGAannotVsHg38v1.ix
html html/TOGAannotation
"
  elif [ -d ${buildDir}/trackData/TOGAvGalGal6v1 ]; then
printf "track HLTOGAannotvGalGal6v1
bigDataUrl bbi/HLTOGAannotVsGalGal6v1.bb
shortLabel TOGA vs. galGal6
longLabel TOGA annotations using chicken/galGal6 as reference
group genes
visibility pack
itemRgb on
type bigBed 12
searchIndex name
searchTrix  ixIxx/HLTOGAannotVsGalGal6v1.ix
html html/TOGAannotation
"
  else
    printf "# ERROR: do not recognize TOGA build directory:\n" 1>&2
    ls -d ${buildDir}/trackData/TOGAv* 1>&2
    exit 255
  fi

$scriptDir/asmHubTOGA.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData > $buildDir/html/TOGAannotation.html

fi	#	if [ "${tg}" -gt 0 ]

# accessionId only used for include statements for other trackDb.txt files
export accessionId="${asmId}"
case ${asmId} in
   GC*)
     accessionId=`echo "$asmId" | awk -F"_" '{printf "%s_%s", $1, $2}'`
     ;;
esac

if [ -s "${buildDir}/$asmId.userTrackDb.txt" ]; then
  printf "\ninclude %s.userTrackDb.txt\n" "${accessionId}"
fi

if [ -s "${buildDir}/$asmId.trackDbOverrides.txt" ]; then
  printf "\ninclude %s.trackDbOverrides.txt\n" "${accessionId}"
fi

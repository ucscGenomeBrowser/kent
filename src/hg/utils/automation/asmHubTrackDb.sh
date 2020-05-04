#!/bin/bash

set -beEux -o pipefail

if [ $# -ne 2 ]; then
  printf "usage: trackDb.sh <asmId> <pathTo/assembly hub build directory> > trackDb.txt\n" 1>&2
  printf "expecting to find *.ucsc.2bit and bbi/ files at given path\n" 1>&2
  printf "the ncbi|ucsc selects the naming scheme\n" 1>&2
  exit 255
fi

export asmId=$1
export buildDir=$2
export hubLinks="/hive/data/genomes/asmHubs/hubLinks"

export scriptDir="$HOME/kent/src/hg/utils/automation"

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
$scriptDir/asmHubAssembly.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/$asmId.agp.gz https://hgdownload.soe.ucsc.edu/hubs/VGP/genomes/$asmId > $buildDir/html/$asmId.assembly.html
fi

if [ -s ${buildDir}/trackData/assemblyGap/${asmId}.gap.bb ]; then
rm -f $buildDir/bbi/${asmId}.gap.bb
ln -s ../trackData/assemblyGap/${asmId}.gap.bb $buildDir/bbi/${asmId}.gap.bb
printf "track gap
longLabel AGP gap
shortLabel Gap (AGP defined)
visibility dense
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

fi

export rmskCount=`(ls $buildDir/trackData/repeatMasker/bbi/${asmId}.rmsk.*.bb | wc -l) || true`


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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.SINE.bb $buildDir/bbi/${asmId}.rmsk.SINE.bb
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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.LINE.bb $buildDir/bbi/${asmId}.rmsk.LINE.bb
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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.LTR.bb $buildDir/bbi/${asmId}.rmsk.LTR.bb
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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.DNA.bb $buildDir/bbi/${asmId}.rmsk.DNA.bb
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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.Simple.bb $buildDir/bbi/${asmId}.rmsk.Simple.bb
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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.Low_complexity.bb $buildDir/bbi/${asmId}.rmsk.Low_complexity.bb
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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.Satellite.bb $buildDir/bbi/${asmId}.rmsk.Satellite.bb
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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.RNA.bb $buildDir/bbi/${asmId}.rmsk.RNA.bb
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
ln -s ../trackData/repeatMasker/bbi/${asmId}.rmsk.Other.bb $buildDir/bbi/${asmId}.rmsk.Other.bb
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

# may or may not have a searchTrix for ncbiGene, assume none
searchTrix=""
# check to see if there is a index for ncbiGene
if [ -s ${buildDir}/trackData/ncbiGene/$asmId.ncbiGene.ix ]; then
  searchTrix="
searchTrix ixIxx/$asmId.ncbiGene.ix"
fi

export haveNcbiGene="no"

if [ -s ${buildDir}/trackData/ncbiGene/$asmId.ncbiGene.bb ]; then
rm -f $buildDir/bbi/${asmId}.ncbiGene.bb
rm -f $buildDir/ixIxx/${asmId}.ncbiGene.ix
rm -f $buildDir/ixIxx/${asmId}.ncbiGene.ixx
ln -s ../trackData/ncbiGene/$asmId.ncbiGene.bb $buildDir/bbi/${asmId}.ncbiGene.bb
ln -s ../trackData/ncbiGene/$asmId.ncbiGene.ix $buildDir/ixIxx/${asmId}.ncbiGene.ix
ln -s ../trackData/ncbiGene/$asmId.ncbiGene.ixx $buildDir/ixIxx/${asmId}.ncbiGene.ixx
  printf "track ncbiGene
longLabel NCBI gene predictions
shortLabel NCBI Genes
visibility pack
color 0,80,150
altColor 150,80,0
colorByStrand 0,80,150 150,80,0
bigDataUrl bbi/%s.ncbiGene.bb
type bigGenePred
html html/%s.ncbiGene
searchIndex name%s
url https://www.ncbi.nlm.nih.gov/gene/?term=\$\$
urlLabel Entrez gene
labelFields geneName,geneName2
group genes\n\n" "${asmId}" "${asmId}" "${searchTrix}"

  $scriptDir/asmHubNcbiGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData > $buildDir/html/$asmId.ncbiGene.html

haveNcbiGene="yes"
fi

###################################################################
# ncbiRefSeq composite track
if [ -s ${buildDir}/trackData/ncbiRefSeq/$asmId.ncbiRefSeq.bb ]; then
rm -f $buildDir/bbi/${asmId}.ncbiRefSeq.bb
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeq.ix
rm -f $buildDir/ixIxx/${asmId}.ncbiRefSeq.ixx
ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeq.bb $buildDir/bbi/${asmId}.ncbiRefSeq.bb
ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeq.ix $buildDir/ixIxx/${asmId}.ncbiRefSeq.ix
ln -s ../trackData/ncbiRefSeq/$asmId.ncbiRefSeq.ixx $buildDir/ixIxx/${asmId}.ncbiRefSeq.ixx

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
noInherit on
allButtonPair on
dataVersion $dataVersion
html html/%s.refSeqComposite
priority 2

        track ncbiRefSeq
        parent refSeqComposite on
        color 12,12,120
        altColor 120,12,12
        shortLabel RefSeq All
        type bigGenePred
        labelFields name,geneName,geneName2
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
        labelFields name,geneName,geneName2
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
        labelFields name,geneName,geneName2
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
        labelFields gene
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

fi	# ncbiRefSeq composite track

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
visibility pack
type bed 3 .
noInherit on
html html/%s.cpgIslands\n\n" "${asmId}"
fi

if [ -s ${buildDir}/trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ]; then
ln -s ../trackData/cpgIslands/masked/${asmId}.cpgIslandExt.bb ${buildDir}/bbi/${asmId}.cpgIslandExt.bb
printf "    track cpgIslandExt
    parent cpgIslands %s
    shortLabel CpG Islands
    longLabel CpG Islands (Islands < 300 Bases are Light Green)
    priority 1
    type bigBed 4 +
    bigDataUrl bbi/%s.cpgIslandExt.bb\n\n" "${cpgVis}" "${asmId}"
fi

if [ -s ${buildDir}/trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb ]; then
ln -s ../trackData/cpgIslands/unmasked/${asmId}.cpgIslandExtUnmasked.bb ${buildDir}/bbi/${asmId}.cpgIslandExtUnmasked.bb
printf "    track cpgIslandExtUnmasked
    parent cpgIslands on
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
visibility dense
type bigBed 3
bigDataUrl bbi/%s.allGaps.bb
html html/%s.allGaps\n\n" "${asmId}" "${asmId}"
$scriptDir/asmHubAllGaps.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/$asmId.agp.gz https://hgdownload.soe.ucsc.edu/hubs/VGP/genomes/$asmId $buildDir/bbi/$asmId > $buildDir/html/$asmId.allGaps.html
fi

###################################################################
# augustus genes
if [ -s ${buildDir}/trackData/augustus/${asmId}.augustus.bb ]; then
rm -f ${buildDir}/bbi/${asmId}.augustus.bb
ln -s ../trackData/augustus/${asmId}.augustus.bb ${buildDir}/bbi/${asmId}.augustus.bb

export augustusVis="dense"

if [ "${haveNcbiGene}" = "no" ]; then
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

###################################################################
# xenoRefGene genes
if [ -s ${buildDir}/trackData/xenoRefGene/${asmId}.xenoRefGene.bb ]; then
rm -f $buildDir/ixIxx/${asmId}.xenoRefGene.ix
rm -f $buildDir/ixIxx/${asmId}.xenoRefGene.ixx
rm -f ${buildDir}/bbi/${asmId}.xenoRefGene.bb
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
labelFields name,geneName,geneName2
searchIndex name
searchTrix ixIxx/%s.xenoRefGene.ix
html html/%s.xenoRefGene\n\n" "${asmId}" "${asmId}" "${asmId}"
$scriptDir/asmHubXenoRefGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/trackData > $buildDir/html/$asmId.xenoRefGene.html
fi

###################################################################
# gapOverlap
# if [ -s ${buildDir}/trackData/gapOverlap/${asmId}.gapOverlap.bb ]; then
# rm -f ${buildDir}/bbi/${asmId}.gapOverlap.bb
# ln -s ../trackData/gapOverlap/${asmId}.gapOverlap.bb ${buildDir}/bbi/${asmId}.gapOverlap.bb

# printf "track gapOverlap
# shortLabel Gap Overlaps
# longLabel Exactly identical sequence on each side of a gap
# group map
# visibility hide
# type bigBed 12 .
# bigDataUrl bbi/%s.gapOverlap.bb
# html html/%s.gapOverlap\n\n" "${asmId}" "${asmId}"

# $scriptDir/asmHubGapOverlap.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.gapOverlap.html

# fi
###################################################################
# Ensembl genes
if [ -s ${buildDir}/trackData/ensGene/bbi/${asmId}.ensGene.bb ]; then
ls -og ${buildDir}/trackData/ensGene/bbi/${asmId}.ensGene.bb 1>&2
printf "# link: ../trackData/ensGene/bbi/${asmId}.ensGene.bb ${buildDir}/bbi/${asmId}.ensGene.bb\n" 1>&2
rm -f ${buildDir}/bbi/${asmId}.ensGene.bb
ln -s ../trackData/ensGene/bbi/${asmId}.ensGene.bb ${buildDir}/bbi/${asmId}.ensGene.bb
rm -f ${buildDir}/ixIxx/${asmId}.ensGene.ix
rm -f ${buildDir}/ixIxx/${asmId}.ensGene.ixx

ln -s ../trackData/ensGene/process/${asmId}.ensGene.ix $buildDir/ixIxx/${asmId}.ensGene.ix
ln -s ../trackData/ensGene/process/${asmId}.ensGene.ixx $buildDir/ixIxx/${asmId}.ensGene.ixx

export ensVersion="v86"

if [ -s ${buildDir}/trackData/ensGene/version.txt ]; then
  ensVersion=`cat "${buildDir}/trackData/ensGene/version.txt"`
fi

printf "track ensGene
shortLabel Ensembl genes
longLabel Ensembl genes %s
group genes
priority 40
visibility pack
color 150,0,0
type bigBed 12 .
bigDataUrl bbi/%s.ensGene.bb
searchIndex name
searchTrix ixIxx/%s.ensGene.ix
html html/%s.ensGene\n\n" "${ensVersion}" "${asmId}" "${asmId}" "${asmId}"

$scriptDir/asmHubEnsGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.ensGene.html "${ensVersion}"

else
  printf "# no ensGene found\n" 1>&2
fi

if [ -s ${hubLinks}/${asmId}/rnaSeqData/$asmId.trackDb.txt ]; then
  printf "include rnaSeqData/%s.trackDb.txt\n\n" "${asmId}"
fi

#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 2 ]; then
  printf "usage: asmHubChainNetTrackDb.pl <asmId> <pathTo/assembly hub build directory> > chainNettrackDb.txt\n" 1>&2
  printf "expecting to find *.ucsc.2bit and bbi/ files at given path\n" 1>&2
  printf "the ncbi|ucsc selects the naming scheme\n" 1>&2
  exit 255
fi

export asmId=$1
export buildDir=$2
export hubLinks="/hive/data/genomes/asmHubs/hubLinks"
export accessionId=`echo "$asmId" | awk -F"_" '{printf "%s_%s", $1, $2}'`

export scriptDir="$HOME/kent/src/hg/utils/automation"

mkdir -p $buildDir/bbi
mkdir -p $buildDir/ixIxx

for D in ${buildDir}/trackData/lastz.*
do
  targetDb=$accessionId
  lastzDir=`basename "${D}"`
  otherDb=`echo $lastzDir | sed -e 's/lastz.//;'`
  OtherDb="${otherDb^}"
  asmReport=`ls -d $buildDir/download/*assembly_report.txt 2> /dev/null`
  if [ ! -s "${asmReport}" ]; then
 printf "# ERROR: can not find assembly_report.txt in $buildDir/download\n" 1>&2
    exit 255
  fi
  if [ ! -s ${buildDir}/trackData/$lastzDir/axtChain/chain${OtherDb}.bb ]; then
 printf "# ERROR: can not find chain${OtherDb}.bb in $buildDir/trackData/$lastzDir/axtChain/\n" 1>&2
    exit 255
  fi
  rm -f $buildDir/bbi/${asmId}.chain$OtherDb.bb
  rm -f $buildDir/bbi/${asmId}.chain${OtherDb}Link.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.net.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.net.summary.bb
  ln -s ../trackData/$lastzDir/axtChain/chain${OtherDb}.bb $buildDir/bbi/${asmId}.chain$OtherDb.bb
  ln -s ../trackData/$lastzDir/axtChain/chain${OtherDb}Link.bb $buildDir/bbi/${asmId}.chain${OtherDb}Link.bb
  ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.net.bb $buildDir/bbi/${asmId}.$otherDb.net.bb
  ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.net.summary.bb $buildDir/bbi/${asmId}.$otherDb.net.summary.bb

  otherPrefix=`echo $otherDb | cut -c1-2`
  if [ "${otherPrefix}" = "GC" ]; then
    sciName=`grep -i 'organism name:' ${asmReport} | head -1 | tr -d "" | sed -e 's/.*organism name: *//i; s/ *(.*//;'`
    organism=`grep -i 'organism name:' ${asmReport} | head -1 | tr -d "" | sed -e 's/.*organism name: *.*(//i; s/).*//;'`
    taxId=`grep -i 'taxid' ${asmReport} | head -1 | tr -d "" | sed -e 's/.*taxid: *//i;'`
    ymd=`grep -i 'date' ${asmReport} | head -1 | tr -d "" | sed -e 's/.*date: *//i;'`
  else
    organism=`hgsql -N -e "select organism from dbDb where name=\"$otherDb\"" hgcentraltest`
    sciName=`hgsql -N -e "select scientificName from dbDb where name=\"$otherDb\"" hgcentraltest`
    taxId=`hgsql -N -e "select taxId from dbDb where name=\"$otherDb\"" hgcentraltest`
    o_date=`hgsql -N -e "select description from dbDb where name=\"$otherDb\"" hgcentraltest`
    matrix=`~/kent/src/hg/utils/phyloTrees/findScores.pl $otherDb $targetDb 2>&1 | grep matrix`
    minScore=`~/kent/src/hg/utils/phyloTrees/findScores.pl $otherDb $targetDb 2>&1 | grep MinScore`
   linGap=`~/kent/src/hg/utils/phyloTrees/findScores.pl $otherDb $targetDb 2>&1 | grep LinearGap`
  fi
  printf "##############################################################################
# $otherDb - $organism - $sciName - taxId: $taxId
##############################################################################
"
  printf "track chainNet$OtherDb
compositeTrack on
shortLabel $organism Chain/Net
longLabel $organism ($o_date), Chain and Net Alignments
subGroup1 view Views chain=Chain net=Net
dragAndDrop subTracks
visibility hide
group compGeno
noInherit on
"

  printf "priority 100.1
color 0,0,0
altColor 100,50,0
type bed 3
sortOrder view=+
"
printf "$matrix
$minScore
$linGap
matrixHeader A, C, G, T
otherDb $otherDb
html chainNet

"
printf "    track chainNet${OtherDb}Viewchain
    shortLabel Chain
    view chain
    visibility pack
    parent chainNet$OtherDb
    spectrum on

        track chain$OtherDb
        parent chainNet${OtherDb}Viewchain
        subGroups view=chain
        shortLabel $organism Chain
        longLabel $organism ($o_date) Chained Alignments
        type bigChain $otherDb
        bigDataUrl bbi/$asmId.chain$OtherDb.bb
        linkDataUrl bbi/$asmId.chain${OtherDb}Link.bb
        html html/$asmId.chainNet

"

printf "    track mafNet${OtherDb}Viewnet
    shortLabel Net
    view net
    visibility full
    parent chainNet$OtherDb

        track net$OtherDb
        parent mafNet${OtherDb}Viewnet
        subGroups view=net
        shortLabel $organism mafNet
        longLabel $organism ($o_date) mafNet Alignment
        type bigMaf
        bigDataUrl bbi/$asmId.$otherDb.net.bb
        summary bbi/$asmId.$otherDb.net.summary.bb
        speciesOrder $otherDb
        html chainNet

"

done

exit 255

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



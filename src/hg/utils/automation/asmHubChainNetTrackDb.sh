#!/bin/bash

set -beEu -o pipefail
set -x

if [ $# -ne 2 ]; then
  printf "usage: asmHubChainNetTrackDb.pl <asmId> <pathTo/assembly hub build directory> > chainNettrackDb.txt\n" 1>&2
  printf "expecting to find *.ucsc.2bit and bbi/ files at given path\n" 1>&2
  printf "the ncbi|ucsc selects the naming scheme\n" 1>&2
  exit 255
fi

export asmId=$1
export buildDir=$2
export hubLinks="/hive/data/genomes/asmHubs/hubLinks"

export accessionId="${asmId}"
case ${asmId} in
   GC*)
     accessionId=`echo "$asmId" | awk -F"_" '{printf "%s_%s", $1, $2}'`
     ;;
esac


export scriptDir="$HOME/kent/src/hg/utils/automation"

mkdir -p $buildDir/bbi
mkdir -p $buildDir/liftOver
mkdir -p $buildDir/quickLift

export chainNetPriority=1

printf "# asmHubChainNetTrackDb.pl $asmId $buildDir\n" 1>&2

for D in ${buildDir}/trackData/lastz.*
do
  targetDb=$accessionId
  lastzDir=`basename "${D}"`
  otherDb=`echo $lastzDir | sed -e 's/lastz.//;'`
  OtherDb="${otherDb^}"
  asmReport=`ls -d $buildDir/download/*assembly_report.txt 2> /dev/null`
  overChain="${targetDb}.${otherDb}.over.chain.gz"
  overToChain="${targetDb}To${OtherDb}.over.chain.gz"
printf "asmReport: %s\n" "${asmReport}" 1>&2
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
  rm -f $buildDir/bbi/${asmId}.chainSyn$OtherDb.bb
  rm -f $buildDir/bbi/${asmId}.chainSyn${OtherDb}Link.bb
  rm -f $buildDir/bbi/${asmId}.chainRBest$OtherDb.bb
  rm -f $buildDir/bbi/${asmId}.chainRBest${OtherDb}Link.bb
  rm -f $buildDir/bbi/${asmId}.chainLiftOver$OtherDb.bb
  rm -f $buildDir/bbi/${asmId}.chainLiftOver${OtherDb}Link.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.net.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.net.summary.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.synNet.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.synNet.summary.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.rbestNet.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.rbestNet.summary.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.liftOverNet.bb
  rm -f $buildDir/bbi/${asmId}.$otherDb.liftOverNet.summary.bb
  rm -f $buildDir/liftOver/${overToChain}
  rm -f $buildDir/quickLift/${otherDb}.bb
  rm -f $buildDir/quickLift/${otherDb}.link.bb
  if [ -s "$buildDir/trackData/$lastzDir/axtChain/${overChain}" ]; then
     ln -s ../trackData/$lastzDir/axtChain/${overChain} $buildDir/liftOver/${overToChain}
  fi
  if [ -s "$buildDir/trackData/$lastzDir/axtChain/chain${OtherDb}.bb" ]; then
    ln -s ../trackData/$lastzDir/axtChain/chain${OtherDb}.bb $buildDir/bbi/${asmId}.chain$OtherDb.bb
    ln -s ../trackData/$lastzDir/axtChain/chain${OtherDb}Link.bb $buildDir/bbi/${asmId}.chain${OtherDb}Link.bb
  fi
  if [ -s "$buildDir/trackData/$lastzDir/bigMaf/$accessionId.$otherDb.net.bb" ]; then
    ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.net.bb $buildDir/bbi/${asmId}.$otherDb.net.bb
    ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.net.summary.bb $buildDir/bbi/${asmId}.$otherDb.net.summary.bb
  fi
  if [ -s "$buildDir/trackData/$lastzDir/axtChain/chainSyn${OtherDb}.bb" ]; then
    ln -s ../trackData/$lastzDir/axtChain/chainSyn${OtherDb}.bb $buildDir/bbi/${asmId}.chainSyn$OtherDb.bb
    ln -s ../trackData/$lastzDir/axtChain/chainSyn${OtherDb}Link.bb $buildDir/bbi/${asmId}.chainSyn${OtherDb}Link.bb
  fi
  if [ -s "$buildDir/trackData/$lastzDir/bigMaf/$accessionId.$otherDb.synNet.bb" ]; then
    ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.synNet.bb $buildDir/bbi/${asmId}.$otherDb.synNet.bb
    ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.synNet.summary.bb $buildDir/bbi/${asmId}.$otherDb.synNet.summary.bb
  fi
  if [ -s "$buildDir/trackData/$lastzDir/axtChain/chainRBest${OtherDb}.bb" ]; then
    ln -s ../trackData/$lastzDir/axtChain/chainRBest${OtherDb}.bb $buildDir/bbi/${asmId}.chainRBest$OtherDb.bb
    ln -s ../trackData/$lastzDir/axtChain/chainRBest${OtherDb}Link.bb $buildDir/bbi/${asmId}.chainRBest${OtherDb}Link.bb
  fi
  if [ -s "$buildDir/trackData/$lastzDir/bigMaf/$accessionId.$otherDb.rbestNet.bb" ]; then
    ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.rbestNet.bb $buildDir/bbi/${asmId}.$otherDb.rbestNet.bb
    ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.rbestNet.summary.bb $buildDir/bbi/${asmId}.$otherDb.rbestNet.summary.bb
  fi

  if [ -s "$buildDir/trackData/$lastzDir/axtChain/chainLiftOver${OtherDb}.bb" ]; then
    printf "# making chainLiftOver${OtherDb}.bb\n" 1>&2
    ln -s ../trackData/$lastzDir/axtChain/chainLiftOver${OtherDb}.bb $buildDir/bbi/${asmId}.chainLiftOver$OtherDb.bb
    ln -s ../trackData/$lastzDir/axtChain/chainLiftOver${OtherDb}Link.bb $buildDir/bbi/${asmId}.chainLiftOver${OtherDb}Link.bb
    if [ -s "$buildDir/trackData/$lastzDir/bigMaf/${accessionId}.${otherDb}.liftOverNet.bb" ]; then
      ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.liftOverNet.bb $buildDir/bbi/${asmId}.$otherDb.liftOverNet.bb
      ln -s ../trackData/$lastzDir/bigMaf/$accessionId.$otherDb.liftOverNet.summary.bb $buildDir/bbi/${asmId}.$otherDb.liftOverNet.summary.bb
    fi
  else
    printf "# there is NO chainLiftOver${OtherDb}.bb\n" 1>&2
  fi

  if [ -s "$buildDir/trackData/$lastzDir/axtChain/${accessionId}.${otherDb}.quick.bb" ]; then
    printf "# making quickLift/${otherDb}.bb\n" 1>&2
    ln -s ../trackData/$lastzDir/axtChain/${accessionId}.${otherDb}.quick.bb $buildDir/quickLift/${otherDb}.bb
    ln -s ../trackData/$lastzDir/axtChain/${accessionId}.${otherDb}.quickLink.bb $buildDir/quickLift/${otherDb}.link.bb
  else
    printf "# there is NO chainQuickLift${OtherDb}.bb\n" 1>&2
  fi

  otherPrefix=`echo $otherDb | cut -c1-2`
  if [ "${otherPrefix}" = "GC" ]; then
    sciName=`grep -i 'organism name:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*organism name: *//i; s/ *(.*//;'`
    organism=`grep -i 'organism name:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*organism name: *.*(//i; s/).*//;'`
    taxId=`grep -i 'taxid:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*taxid: *//i;'`
    o_date=`grep -i 'date:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*date: *//i;'`
    matrix=""
    linGap=""
    minScore=""
  else
    organism=`hgsql -N -e "select organism from dbDb where name=\"$otherDb\"" hgcentraltest`
    sciName=`hgsql -N -e "select scientificName from dbDb where name=\"$otherDb\"" hgcentraltest`
    taxId=`hgsql -N -e "select taxId from dbDb where name=\"$otherDb\"" hgcentraltest`
    o_date=`hgsql -N -e "select description from dbDb where name=\"$otherDb\"" hgcentraltest`
    matrix=`~/kent/src/hg/utils/phyloTrees/findScores.pl $otherDb $targetDb 2>&1 | grep matrix`
  fi

  otherPrefix=`echo $otherDb | cut -c1-2`
  if [ "${otherPrefix}" = "GC" ]; then
    sciName=`grep -i 'organism name:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*organism name: *//i; s/ *(.*//;'`
    organism=`grep -i 'organism name:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*organism name: *.*(//i; s/).*//;'`
    taxId=`grep -i 'taxid:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*taxid: *//i;'`
    o_date=`grep -i 'date:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*date: *//i;'`
    matrix=""
    linGap=""
    minScore=""
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
"

  printf "priority 100.1
color 0,0,0
altColor 100,50,0
type bed 3
sortOrder view=+
"
  if [ "x${matrix}y" = "xy" ]; then
printf "otherDb $otherDb
html html/$asmId.chainNet

"
else
printf "$matrix
$minScore
$linGap
matrixHeader A, C, G, T
otherDb $otherDb
html html/$asmId.chainNet

"
fi
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
        priority %d

" $((chainNetPriority++))

if [ -s "$buildDir/bbi/${asmId}.chainSyn$OtherDb.bb" ]; then

printf "        track chainSyn$OtherDb
        parent chainNet${OtherDb}Viewchain
        subGroups view=chain
        shortLabel $organism synChain
        longLabel $organism ($o_date) Syntenic Chained Alignments
        type bigChain $otherDb
        bigDataUrl bbi/$asmId.chainSyn$OtherDb.bb
        linkDataUrl bbi/$asmId.chainSyn${OtherDb}Link.bb
        priority %d

" $((chainNetPriority++))

fi

if [ -s "$buildDir/bbi/${asmId}.chainRBest$OtherDb.bb" ]; then

printf "        track chainRBest$OtherDb
        parent chainNet${OtherDb}Viewchain
        subGroups view=chain
        shortLabel $organism rbChain
        longLabel $organism ($o_date) Reciprocal Best Chained Alignments
        type bigChain $otherDb
        bigDataUrl bbi/$asmId.chainRBest$OtherDb.bb
        linkDataUrl bbi/$asmId.chainRBest${OtherDb}Link.bb
        priority %d

" $((chainNetPriority++))

fi

if [ -s "$buildDir/bbi/${asmId}.chainLiftOver$OtherDb.bb" ]; then

printf "        track chainLiftOver$OtherDb
        parent chainNet${OtherDb}Viewchain
        subGroups view=chain
        shortLabel $organism loChain
        longLabel $organism ($o_date) Lift Over Chained Alignments
        type bigChain $otherDb
        bigDataUrl bbi/$asmId.chainLiftOver$OtherDb.bb
        linkDataUrl bbi/$asmId.chainLiftOver${OtherDb}Link.bb
        priority %d

" $((chainNetPriority++))

fi

printf "    track mafNet${OtherDb}Viewnet
    shortLabel Net
    view net
    visibility full
    parent chainNet$OtherDb

        track net$OtherDb
        parent mafNet${OtherDb}Viewnet
        subGroups view=net
        shortLabel $organism net
        longLabel $organism ($o_date) Net Alignment
        type bigMaf
        bigDataUrl bbi/$asmId.$otherDb.net.bb
        summary bbi/$asmId.$otherDb.net.summary.bb
        speciesOrder $otherDb
        priority %d

" $((chainNetPriority++))

if [ -s "$buildDir/bbi/${asmId}.$otherDb.synNet.summary.bb" ]; then

printf "        track synNet$OtherDb
        parent mafNet${OtherDb}Viewnet
        subGroups view=net
        shortLabel $organism synNet
        longLabel $organism ($o_date) Syntenic Net Alignment
        type bigMaf
        bigDataUrl bbi/$asmId.$otherDb.synNet.bb
        summary bbi/$asmId.$otherDb.synNet.summary.bb
        speciesOrder $otherDb
        priority %d

" $((chainNetPriority++))

fi

if [ -s "$buildDir/bbi/${asmId}.$otherDb.rbestNet.summary.bb" ]; then

printf "        track rbestNet$OtherDb
        parent mafNet${OtherDb}Viewnet
        subGroups view=net
        shortLabel $organism rbestNet
        longLabel $organism ($o_date) Reciprocal Best Net Alignment
        type bigMaf
        bigDataUrl bbi/$asmId.$otherDb.rbestNet.bb
        summary bbi/$asmId.$otherDb.rbestNet.summary.bb
        speciesOrder $otherDb
        priority %d

" $((chainNetPriority++))

fi

if [ -s "$buildDir/bbi/${asmId}.$otherDb.liftOverNet.summary.bb" ]; then

printf "        track liftOverNet$OtherDb
        parent mafNet${OtherDb}Viewnet
        subGroups view=net
        shortLabel $organism liftOverNet
        longLabel $organism ($o_date) Lift Over Net Alignment
        type bigMaf
        bigDataUrl bbi/$asmId.$otherDb.liftOverNet.bb
        summary bbi/$asmId.$otherDb.liftOverNet.summary.bb
        speciesOrder $otherDb
        priority %d

" $((chainNetPriority++))

fi

done

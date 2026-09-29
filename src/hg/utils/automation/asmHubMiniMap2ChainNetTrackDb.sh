#!/bin/bash

set -beEu -o pipefail
set -x

if [ $# -ne 2 ]; then
  printf "usage: asmHubMiniMap2ChainNetTrackDb.sh <asmId> <pathTo/assembly hub build directory> > miniMap2ChainNetTrackDb.txt\n" 1>&2
  printf "expecting to find bbi/ files at given path\n" 1>&2
  printf "single query genome case, use asmHubMiniMap2ChainNetTrackDb.pl for multiple\n" 1>&2
  exit 255
fi

export asmId=$1
export buildDir=$2

export accessionId="${asmId}"
case ${asmId} in
   GC*)
     accessionId=`echo "$asmId" | awk -F"_" '{printf "%s_%s", $1, $2}'`
     ;;
esac

export scriptDir="$HOME/kent/src/hg/utils/automation"

mkdir -p $buildDir/bbi

export chainNetPriority=1

printf "# asmHubMiniMap2ChainNetTrackDb.sh $asmId $buildDir\n" 1>&2

for D in ${buildDir}/trackData/miniMap2.*
do
  targetDb=$accessionId
  mm2Dir=`basename "${D}"`
  # doMiniMap2.pl names this directory 'miniMap2.$QDb' where $QDb is
  # ucfirst() of the query db/accession; OtherDb below reproduces the
  # exact case used in the chain*.bb file names, otherDb reconstructs
  # the actual lowercase-first db name/accession for table lookups.
  OtherDb=`echo $mm2Dir | sed -e 's/miniMap2.//;'`
  dbPrefix2=`printf '%s' "$OtherDb" | cut -c1-2`
  if [ "${dbPrefix2}" = "GC" ]; then
    otherDb="${OtherDb}"
  else
    firstChar=`printf '%s' "$OtherDb" | cut -c1`
    restChars=`printf '%s' "$OtherDb" | cut -c2-`
    lowerFirst=`printf '%s' "$firstChar" | tr 'A-Z' 'a-z'`
    otherDb="${lowerFirst}${restChars}"
  fi
  asmReport=`ls -d $buildDir/download/*assembly_report.txt 2> /dev/null`
printf "asmReport: %s\n" "${asmReport}" 1>&2
  if [ ! -s "${asmReport}" ]; then
 printf "# ERROR: can not find assembly_report.txt in $buildDir/download\n" 1>&2
    exit 255
  fi
  if [ ! -s ${buildDir}/trackData/$mm2Dir/axtChain/chain${OtherDb}.bb ]; then
 printf "# ERROR: can not find chain${OtherDb}.bb in $buildDir/trackData/$mm2Dir/axtChain/\n" 1>&2
    exit 255
  fi
  rm -f $buildDir/bbi/${asmId}.chainMiniMap2$OtherDb.bb
  rm -f $buildDir/bbi/${asmId}.chainMiniMap2${OtherDb}Link.bb
  rm -f $buildDir/bbi/${asmId}.chainLiftOverMiniMap2$OtherDb.bb
  rm -f $buildDir/bbi/${asmId}.chainLiftOverMiniMap2${OtherDb}Link.bb

  ln -s ../trackData/$mm2Dir/axtChain/chain${OtherDb}.bb $buildDir/bbi/${asmId}.chainMiniMap2$OtherDb.bb
  ln -s ../trackData/$mm2Dir/axtChain/chain${OtherDb}Link.bb $buildDir/bbi/${asmId}.chainMiniMap2${OtherDb}Link.bb

  if [ -s "$buildDir/trackData/$mm2Dir/axtChain/chainLiftOver${OtherDb}.bb" ]; then
    printf "# making chainLiftOverMiniMap2${OtherDb}.bb\n" 1>&2
    ln -s ../trackData/$mm2Dir/axtChain/chainLiftOver${OtherDb}.bb $buildDir/bbi/${asmId}.chainLiftOverMiniMap2$OtherDb.bb
    ln -s ../trackData/$mm2Dir/axtChain/chainLiftOver${OtherDb}Link.bb $buildDir/bbi/${asmId}.chainLiftOverMiniMap2${OtherDb}Link.bb
  else
    printf "# there is NO chainLiftOver${OtherDb}.bb\n" 1>&2
  fi

  otherPrefix=`echo $otherDb | cut -c1-2`
  if [ "${otherPrefix}" = "GC" ]; then
    # Construct path to query assembly directory: /hive/data/genomes/asmHubs/GCF/000/001/635/GCF_000001635.27/
    gcPrefix=`echo $otherDb | sed -e 's/_.*//'`
    numbers=`echo $otherDb | sed -e 's/GC[AF]_//; s/\.[0-9]*$//'`
    d1=`echo $numbers | cut -c1-3`
    d2=`echo $numbers | cut -c4-6`
    d3=`echo $numbers | cut -c7-9`
    queryBuildDir="/hive/data/genomes/asmHubs/$gcPrefix/$d1/$d2/$d3/$otherDb"
    queryAsmReport=`ls -d $queryBuildDir/*assembly_report.txt 2> /dev/null || true`
    if [ -s "${queryAsmReport}" ]; then
      sciName=`grep -i 'organism name:' ${queryAsmReport} | head -1 | tr -d "\r" | sed -e 's/.*organism name: *//i; s/ *(.*//;'`
      organism=`grep -i 'organism name:' ${queryAsmReport} | head -1 | tr -d "\r" | sed -e 's/.*organism name: *.*(//i; s/).*//;'`
      taxId=`grep -i 'taxid:' ${queryAsmReport} | head -1 | tr -d "\r" | sed -e 's/.*taxid: *//i;'`
      o_date=`grep -i 'date:' ${queryAsmReport} | head -1 | tr -d "\r" | sed -e 's/.*date: *//i;'`
    else
      printf "# ERROR: can not find assembly_report.txt for query assembly $otherDb in $queryBuildDir/\n" 1>&2
      exit 255
    fi
  else
    organism=`/cluster/bin/x86_64/hgsql -N -e "select organism from dbDb where name=\"$otherDb\"" hgcentraltest`
    sciName=`/cluster/bin/x86_64/hgsql -N -e "select scientificName from dbDb where name=\"$otherDb\"" hgcentraltest`
    taxId=`/cluster/bin/x86_64/hgsql -N -e "select taxId from dbDb where name=\"$otherDb\"" hgcentraltest`
    o_date=`/cluster/bin/x86_64/hgsql -N -e "select description from dbDb where name=\"$otherDb\"" hgcentraltest`
  fi

  printf "##############################################################################
# $otherDb - $organism - $sciName - taxId: $taxId
##############################################################################
"
  printf "track chainNetMiniMap2$OtherDb
compositeTrack on
shortLabel $organism miniMap2 Chain/Net
longLabel $organism ($o_date), minimap2 Chain and Lift Over Alignments
subGroup1 view Views chain=Chain liftover=LiftOver
dragAndDrop subTracks
visibility hide
group compGeno
"

  printf "priority 100.1
color 0,0,0
altColor 100,50,0
type bed 3
sortOrder view=+
otherDb $otherDb
html html/$asmId.miniMap2ChainNet

"

  printf "    track chainNetMiniMap2${OtherDb}Viewchain
    shortLabel Chain
    view chain
    visibility pack
    parent chainNetMiniMap2$OtherDb
    spectrum on

        track chainMiniMap2$OtherDb
        parent chainNetMiniMap2${OtherDb}Viewchain
        subGroups view=chain
        shortLabel $organism mm2 Chain
        longLabel $organism ($o_date) minimap2 Chained Alignments
        type bigChain $otherDb
        bigDataUrl bbi/$asmId.chainMiniMap2$OtherDb.bb
        linkDataUrl bbi/$asmId.chainMiniMap2${OtherDb}Link.bb
        priority %d

" $((chainNetPriority++))

if [ -s "$buildDir/bbi/${asmId}.chainLiftOverMiniMap2$OtherDb.bb" ]; then

printf "    track chainNetMiniMap2${OtherDb}ViewLiftOver
    shortLabel Lift Over
    view liftover
    visibility hide
    parent chainNetMiniMap2$OtherDb
    spectrum on

        track chainLiftOverMiniMap2$OtherDb
        parent chainNetMiniMap2${OtherDb}ViewLiftOver
        subGroups view=liftover
        shortLabel $organism mm2 loChain
        longLabel $organism ($o_date) minimap2 Lift Over Chained Alignments
        type bigChain $otherDb
        bigDataUrl bbi/$asmId.chainLiftOverMiniMap2$OtherDb.bb
        linkDataUrl bbi/$asmId.chainLiftOverMiniMap2${OtherDb}Link.bb
        priority %d

" $((chainNetPriority++))

fi

done

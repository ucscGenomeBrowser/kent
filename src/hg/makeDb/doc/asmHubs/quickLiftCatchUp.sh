#!/bin/bash
#######################################################################
### quickLiftCatchUp.sh <target> <query>
#######################################################################

set -beEu -o pipefail

if [ $# -ne 2 ]; then
  printf "usage: ./quickLiftCatchUp.sh targetAsmId queryAsmId\n" 1>&2

  printf "Helper script to catch up and create quickLift files, symLinks and
quickLiftChain table entry if it hasn't been done before by
the usual lastz/chain/net process.\n" 1>&2
  exit 255
fi

# export S=`echo $((15 + RANDOM % (85 - 15 + 1)))`
# sleep $S

export target="${1}"
export query="${2}"

export targetDir="${target}"
export targetAcc="${target}"
export targetSizes="chrom.sizes"
export qlPath="/gbdb/genark/"
export quickLinkSrc=""
if [[ $target == GC* ]]; then
    gcX="${target:0:3}"
    d0="${target:4:3}"
    d1="${target:7:3}"
    d2="${target:10:3}"
    targetAcc=`echo $target | cut -d'_' -f1-2`
    tDir="/hive/data/genomes/asmHubs/allBuild/$gcX/$d0/$d1/$d2/$target"
    targetDir="`realpath ${tDir}`/trackData"
    targetSizes="`realpath ${tDir}`/${target}.chrom.sizes"
    qlPath="/gbdb/genark/${gcX}/${d0}/${d1}/${d2}/${targetAcc}/quickLift"
    quickLinkSrc="${targetDir}"
else
    targetDir="/hive/data/genomes/${target}/bed"
    targetSizes="/hive/data/genomes/${target}/chrom.sizes"
    targetAcc="${target}"
    qlPath="/gbdb/${target}/quickLift"
    mkdir -p "${qlPath}"
    quickLinkSrc="/hive/data/genomes/${target}/bed"
fi

if [ ! -d "${targetDir}" ]; then
   printf "ERROR: can not find targetDir: '%s'\n" "${targetDir}" 1>&2
   exit 255
fi

export queryDir="${query}"
export queryAcc="${query}"
export querySizes="chrom.sizes"
export quickLinkPath="${qlPath}"
if [[ $query == GC* ]]; then
    gcX="${query:0:3}"
    d0="${query:4:3}"
    d1="${query:7:3}"
    d2="${query:10:3}"
    queryAcc=`echo $query | cut -d'_' -f1-2`
    tDir="/hive/data/genomes/asmHubs/allBuild/$gcX/$d0/$d1/$d2/$query"
    queryDir="`realpath ${tDir}`/trackData"
    querySizes="`realpath ${tDir}`/${query}.chrom.sizes"
    quickLinkPath="${qlPath}/${queryAcc}"
    quickLinkSrc="${quickLinkSrc}/lastz.${queryAcc}/axtChain/${targetAcc}.${queryAcc}"
else
    queryDir="/hive/data/genomes/${query}/bed/"
    querySizes="/hive/data/genomes/${query}/chrom.sizes"
    quickLinkPath="${qlPath}/${query}"
    quickLinkSrc="${quickLinkSrc}/lastz.${queryAcc}/axtChain/${targetAcc}.${queryAcc}"
fi

export QueryAcc="${queryAcc^}"
export targetQueryOverChain="${targetAcc}.${queryAcc}.over.chain.gz"
export fbTargetQuery="../fb.${targetAcc}.quick${QueryAcc}Link.txt"

if [ ! -d "${queryDir}" ]; then
   printf "ERROR: can not find queryDir: '%s'\n" "${queryDir}" 1>&2
   exit 255
fi

export axtChain="${targetDir}/lastz.${queryAcc}/axtChain"
printf "checking: %s\n" "${axtChain}" 1>&2
printf "checking: %s\n" "${targetQueryOverChain}" 1>&2
if [ ! -d "${axtChain}" ]; then
   axtChain="`ls -d ${targetDir}/blat.${queryAcc}.* | tail -1`"
   if [ -d "${axtChain}" ]; then
     targetQueryOverChain="${axtChain}/${targetAcc}To${QueryAcc}.over.chain.gz"
     fbTargetQuery="fb.${targetAcc}.quick${QueryAcc}Link.txt"
     quickLinkSrc="${axtChain}/${targetAcc}.${queryAcc}"
     if [ ! -s "${targetQueryOverChain}" ]; then
       printf "ERROR: can not find target.query.over.chain:\n%s\n" "${targetQueryOverChain}"
       exit 255
     fi
   else
   printf "ERROR: can not find axtChain: '%s'\n" "${axtChain}" 1>&2
      exit 255
   fi
fi

cd "${axtChain}"

if [ -s "${fbTargetQuery}" ]; then
  printf "DONE: ${target} ${query} %s\n" "`cat ${fbTargetQuery}`" 1>&2
  exit 0
fi

if [ -s "${quickLinkPath}.bb" ]; then
  printf "symLink exists: ${quickLinkPath}.bb\n" 1>&2
  exit 0
fi

if [ ! -s "${targetQueryOverChain}" ]; then
  printf "ERROR: can not find target.query.over.chain:\n%s\n" "${targetQueryOverChain}"
  exit 255
fi

printf "working: %s\n" "${axtChain}" 1>&2
printf "%s\n" "${targetSizes}" 1>&2
printf "%s\n" "${querySizes}" 1>&2
printf "%s\n" "${targetQueryOverChain}" 1>&2
printf "%s\n" "${fbTargetQuery}" 1>&2
printf "%s\n" "${quickLinkPath}" 1>&2
printf "%s\n" "${quickLinkSrc}" 1>&2


if [ "${targetQueryOverChain}" -nt ${targetAcc}.${queryAcc}.quick.chain.txt ]; then
  chainSwap "${targetQueryOverChain}" stdout \
     | chainToBigChain stdin  ${targetAcc}.${queryAcc}.quick.chain.txt \
       ${targetAcc}.${queryAcc}.quick.link.txt

  touch -r "${targetQueryOverChain}" ${targetAcc}.${queryAcc}.quick.chain.txt
  touch -r "${targetQueryOverChain}" ${targetAcc}.${queryAcc}.quick.link.txt
else
  printf "DONE: ${targetAcc}.${queryAcc}.quick.chain.txt\n"
fi

if [ ${targetAcc}.${queryAcc}.quick.chain.txt -nt ${targetAcc}.${queryAcc}.quick.bb ]; then
  bedToBigBed -type=bed6 -as=$HOME/kent/src/hg/lib/bigChain.as \
   -tab ${targetAcc}.${queryAcc}.quick.chain.txt \
    "${querySizes}" \
      ${targetAcc}.${queryAcc}.quick.bb
  touch -r ${targetAcc}.${queryAcc}.quick.chain.txt ${targetAcc}.${queryAcc}.quick.bb
else
  printf "DONE: ${targetAcc}.${queryAcc}.quick.bb\n"
fi

if [ ${targetAcc}.${queryAcc}.quick.link.txt -nt ${targetAcc}.${queryAcc}.quickLink.bb ]; then
  bedToBigBed -type=bed4+1 -as=$HOME/kent/src/hg/lib/bigLink.as \
   -tab ${targetAcc}.${queryAcc}.quick.link.txt \
    "${querySizes}" \
        ${targetAcc}.${queryAcc}.quickLink.bb
  touch -r ${targetAcc}.${queryAcc}.quick.link.txt ${targetAcc}.${queryAcc}.quickLink.bb
else
  printf "DONE: ${targetAcc}.${queryAcc}.quickLink.bb\n"
fi

export totalBases=`ave -col=2 "${querySizes}" | grep "^total" | awk '{printf "%d", $2}'`
export basesCovered=`bigBedInfo ${targetAcc}.${queryAcc}.quickLink.bb | grep basesCovered | cut -d' ' -f2 | tr -d ','`
export percentCovered=`echo $basesCovered $totalBases | awk '{printf "%.3f", 100.0*$1/$2}'`
printf "%d bases of %d (%s%%) in intersection\n" "$basesCovered" "$totalBases" "$percentCovered" > "${fbTargetQuery}"
printf "# %s %s %s\n" "${targetAcc}" "${queryAcc}" "`cat ${fbTargetQuery}`" 1>&2
rm -f ${targetAcc}.${queryAcc}.quick.chain.txt ${targetAcc}.${queryAcc}.quick.link.txt

if [[ $target == GC* ]]; then
    cd "${targetDir}"
    cd ..
    ./doTrackDb.bash
    /cluster/home/hiram/kent/src/hg/utils/automation/addQuickLift.py \
      "${targetAcc}" "${queryAcc}" "${quickLinkPath}.bb"
else
    printf "/cluster/home/hiram/kent/src/hg/utils/automation/addQuickLift.py ${targetAcc} ${queryAcc} ${quickLinkPath}.bb\n" 1>&2
    /cluster/home/hiram/kent/src/hg/utils/automation/addQuickLift.py ${targetAcc} ${queryAcc} ${quickLinkPath}.bb
    printf "ln -s ${quickLinkSrc}.quick.bb ${quickLinkPath}.bb\n"
    printf "ln -s ${quickLinkSrc}.quickLink.bb ${quickLinkPath}.link.bb\n"
    ln -s ${quickLinkSrc}.quick.bb ${quickLinkPath}.bb
    ln -s ${quickLinkSrc}.quickLink.bb ${quickLinkPath}.link.bb
fi

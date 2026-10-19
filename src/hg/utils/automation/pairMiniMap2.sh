#!/bin/bash

set -beEu -o pipefail

export userName="`whoami`"

export bigHub="hgwdev"
export workHorse="hgwdev"
export fileServer="hgwdev"

export doMiniMap2="$HOME/kent/src/hg/utils/automation/doMiniMap2.pl"

# parse optional flags:
#   -force          skip genark table verification
#   -liftTableLoad  passed along to doMiniMap2.pl, allows the liftOver/
#                   quickLift install symlinks (bed/liftOver, goldenPath,
#                   /gbdb) and registration in hgcentral liftOverChain/
#                   quickLiftChain tables
export forceRun=0
export liftTableLoad=""
while [ $# -gt 0 ]; do
  case "$1" in
    -force) forceRun=1; shift ;;
    -liftTableLoad) liftTableLoad="-liftTableLoad"; shift ;;
    *) break ;;
  esac
done

if [ $# != 2 ]; then
  printf "ERROR: arg count: %d != 2\n" "$#" 1>&2

  printf "usage: pairMiniMap2.sh [-force] [-liftTableLoad] <target> <query>

Where target/query is either a UCSC db name, or is an
   assembly hub identifier, e.g.: GCA_011100555.2_mCalJa1.2.pat.X
   (or just the accession: GCA_011100555.2)

These should be two very similar same-species/same-individual
   assemblies (e.g. the two haplotypes of a diploid assembly).
   doMiniMap2.pl will refuse a pair that mash finds too diverged,
   use pairLastz.sh for those.

The -force option skips the hgcentraltest.genark table verification
   for GenArk assembly identifiers.

The -liftTableLoad option is passed along to doMiniMap2.pl to allow
   it to install the liftOver and quickLift chains: the symlinks into
   bed/liftOver/, goldenPath/<db>/liftOver/, /gbdb/<db>/liftOver/ and
   /gbdb/<db>/quickLift/ (or the GenArk hub-build and /gbdb/genark/
   equivalents), and the hgcentral liftOverChain and quickLiftChain
   table entries.  Without it, the results stay in the build
   directories, nothing installed is touched.  Run again with it to
   install a pair that is already done.

Will create directory to work in, for example if, UCSC db:
  /hive/data/genomes/target/bed/miniMap2Query.yyyy-mm-dd/

Or, in the assembly hub build directory:
/hive/data/genomes/asmHubs/genbankBuild/GCA/011/100/555/GCA_011100555.2_mCalJa1.2.pat.X/trackData/miniMap2Query.yyyy-mm-dd

Will set up a run.sh script there to run doMiniMap2.pl and output
  makeDoc text to document what happened.

AND MORE, it will run the doMiniMap2.pl -swap operation into the
  corresponding miniMap2.target.swap directory in the query genome
  work space.

Email will be sent to: '$userName' upon completion.

e.g.: pairMiniMap2.sh GCA_011100555.2 calJac4\n" 1>&2
  exit 255
fi

# asmId - if assembly hub, determine GCx_012345678.9 name
#         if not, return the asmName (== UCSC database name)
function asmId() {
  export asmName=$1
  export id="${asmName}"
  case $asmName in
     GC[AF]_*)
       id=`echo $asmName | cut -d'_' -f1-2`
       ;;
     *)
       ;;
  esac
  printf "%s" "${id}"
}

# gcPath - if assembly hub, determine GCx/012/345/678 path
#          if not return empty string "" (== UCSC database name)
function gcPath() {
  export asmName=$1
  export GCxPath=""
  case $asmName in
     GC[AF]_*)
       gcX=`echo $asmName | cut -c1-3`
       d0=`echo $asmName | cut -c5-7`
       d1=`echo $asmName | cut -c8-10`
       d2=`echo $asmName | cut -c11-13`
       GCxPath="${gcX}/${d0}/${d1}/${d2}"
       ;;
     *)
       ;;
  esac
  printf "%s" "${GCxPath}"
}

# asmBuild - if assembly hub, the actual GenArk build directory, the
#   same resolution as AsmHub::asmBuildDir() used by doMiniMap2.pl:
#   /hive/data/genomes/asmHubs/{genbankBuild,refseqBuild}/GCx/ddd/ddd/ddd/asmId
#   empty string "" if not found, or not an assembly hub
function asmBuild() {
  export asmName=$1
  export buildPath=""
  case $asmName in
     GC[AF]_*)
       acc=$(asmId $asmName)
       hubBuildDir="refseqBuild"
       if [ "${acc:0:3}" = "GCA" ]; then
         hubBuildDir="genbankBuild"
       fi
       buildPath=`ls -d /hive/data/genomes/asmHubs/${hubBuildDir}/$(gcPath $acc)/${acc}_* 2> /dev/null | head -1 || true`
       ;;
     *)
       ;;
  esac
  printf "%s" "${buildPath}"
}

function orgName() {
  export asmName=$1
  case $asmName in
     GC[AF]_*)
       gcxPath=$(gcPath $asmName)
       asmDir="/hive/data/outside/ncbi/genomes/${gcxPath}/${asmName}"
       asmRpt="${asmDir}/${asmName}_assembly_report.txt"
       oName=`(grep -E -m 1 -i "^# organism name:" ${asmRpt} 2> /dev/null || true) | tr -d '\r' | sed -e 's/.*(//; s/).*//'`
       ;;
     *)
       oName=`hgsql -N -e "select organism from dbDb where name=\"${asmName}\";" hgcentraltest`
       ;;
  esac
  printf "%s" "${oName}"
}

function orgDate() {
  export asmName=$1
  case $asmName in
     GC[AF]_*)
       gcxPath=$(gcPath $asmName)
       asmDir="/hive/data/outside/ncbi/genomes/${gcxPath}/${asmName}"
       asmRpt="${asmDir}/${asmName}_assembly_report.txt"
       oDate=`(grep -E -m 1 -i "^#[[:space:]]*Date:" ${asmRpt} 2> /dev/null || true) | tr -d '\r' | sed -e 's/.*ate: \+//;'`
       ;;
     *)
       oDate=""
       ;;
  esac
  printf "%s" "${oDate}"
}

# verifyGenark - verify a GenArk accession exists in hgcentraltest.genark
#   returns 0 if found, 1 if not found
function verifyGenark() {
  local asmAccession=$1
  local fullName=$2
  local count=$(hgsql -N -e "SELECT COUNT(*) FROM genark WHERE gcAccession='${asmAccession}';" hgcentraltest)
  if [ "$count" -eq 0 ]; then
    printf "ERROR: assembly '%s' not found in GenArk\n" "$fullName" 1>&2
    return 1
  fi
  return 0
}

# liftTableRegister - for a direction that was completed by an earlier
#   run without -liftTableLoad, rerun just doMiniMap2.pl's load step
#   with -liftTableLoad to install its liftOver/quickLift symlinks and
#   register it in hgcentral liftOverChain and quickLiftChain.  doLoad()
#   skips the bigBed builds that already exist, and the symlinks and
#   hgAddLiftOverChain/addQuickLift.py are safe to run again.
#   doneDir is the completed build for fromId -> toId, otherDir is the
#   completed build for the opposite direction.  When doneDir is itself
#   a *.swap directory (the reverse pair was the primary run), it is
#   redone as that -swap run, from otherDir.
function liftTableRegister() {
  local doneDir=$1
  local otherDir=$2
  local fromId=$3
  local toId=$4
  local -a cmd
  case "${doneDir}" in
    *.swap)
      if [ -z "${otherDir}" -o ! -d "${otherDir}" ]; then
        printf "ERROR: can not find the primary build for swap directory %s\n" "${doneDir}" 1>&2
        exit 255
      fi
      cmd=("${doMiniMap2}" -swap -liftTableLoad -verbose=2
        -workhorse=${workHorse} -bigClusterHub=${bigHub} -fileServer=${fileServer}
        -buildDir=${otherDir} -swapDir=${doneDir} ${toId} ${fromId})
      ;;
    *)
      cmd=("${doMiniMap2}" -continue=load -stop=load -liftTableLoad -verbose=2
        -workhorse=${workHorse} -bigClusterHub=${bigHub} -fileServer=${fileServer}
        -buildDir=${doneDir} ${fromId} ${toId})
      ;;
  esac
  printf "# running: %s >> %s/liftTableLoad.log 2>&1\n" "${cmd[*]}" "${doneDir}" 1>&2
  if ! { time "${cmd[@]}" ; } >> ${doneDir}/liftTableLoad.log 2>&1; then
    printf "ERROR: -liftTableLoad registration failed, see ${doneDir}/liftTableLoad.log\n" 1>&2
    exit 255
  fi
  printf "\n    # liftOver/quickLift install and hgcentral registration, $DS\n    cd ${doneDir}\n    time (%s) >> liftTableLoad.log 2>&1\n" "${cmd[*]}" >> ${buildDir}/makeDoc.txt
}

# fbLines - show an fb.*.txt file in makeDoc style, if it exists
function fbLines() {
  local fbFile=$1
  if [ -s "${fbFile}" ]; then
    sed -e 's/^/    # /;' "${fbFile}"
  else
    printf "    # (missing: %s)\n" "${fbFile}"
  fi
}

##############################################################################
##############################################################################
### start seconds
export startT=`date "+%s"`

export target="$1"
export query="$2"

# doMiniMap2.pl is given the bare accession for an assembly hub, that is
# what it (and asmHubMiniMap2ChainNet*.pl) use in the miniMap2.<query>
# dateless symlink, and (ucfirst) in the chain and fb.*.txt file names
export tId=$(asmId $target)
export qId=$(asmId $query)

if [ "${tId}" = "${qId}" ]; then
  printf "ERROR: target and query are the same assembly: '%s'\n" "${tId}" 1>&2
  exit 255
fi

# verify GenArk assemblies exist in hgcentraltest.genark unless -force
if [ "$forceRun" -eq 0 ]; then
  export genarkErrors=0
  case $target in
    GC[AF]_*) verifyGenark "$tId" "$target" || genarkErrors=$((genarkErrors+1)) ;;
  esac
  case $query in
    GC[AF]_*) verifyGenark "$qId" "$query" || genarkErrors=$((genarkErrors+1)) ;;
  esac
  if [ "$genarkErrors" -gt 0 ]; then
    printf "Use -force to skip this check\n" 1>&2
    exit 255
  fi
fi

# upper case first character
export Target="${tId^}"
export Query="${qId^}"

# assume UCSC db build, same as doMiniMap2.pl resolveAssemblySeq()/asmRoot()
export tAsmName="${tId}"
export qAsmName="${qId}"
export tRoot="/hive/data/genomes/${tId}/bed"
export qRoot="/hive/data/genomes/${qId}/bed"
export target2bit="/scratch/data/${tId}/${tId}.2bit"
if [ ! -s "${target2bit}" ]; then
  target2bit="/hive/data/genomes/${tId}/${tId}.2bit"
fi
export query2bit="/scratch/data/${qId}/${qId}.2bit"
if [ ! -s "${query2bit}" ]; then
  query2bit="/hive/data/genomes/${qId}/${qId}.2bit"
fi
export targetSizes="/hive/data/genomes/${tId}/chrom.sizes"
export querySizes="/hive/data/genomes/${qId}/chrom.sizes"
export tTdb="xxx"
export qTdb="xxx"

#  override those specifications if assembly hub
case $target in
     GC[AF]_*)
       tAsmBuild=$(asmBuild $target)
       if [ -z "${tAsmBuild}" ]; then
         printf "ERROR: can not find GenArk build directory for '%s'\n" "${target}" 1>&2
         exit 255
       fi
       tAsmName=`basename "${tAsmBuild}"`
       if [ "${target}" != "${tId}" -a "${target}" != "${tAsmName}" ]; then
         printf "ERROR: given target '%s' does not match build directory '%s'\n" "${target}" "${tAsmBuild}" 1>&2
         exit 255
       fi
       tRoot="${tAsmBuild}/trackData"
       target2bit="${tAsmBuild}/${tAsmName}.2bit"
       targetSizes="${tAsmBuild}/${tAsmName}.chrom.sizes"
       tTdb="${tAsmBuild}/doTrackDb.bash"
       ;;
esac

case $query in
     GC[AF]_*)
       qAsmBuild=$(asmBuild $query)
       if [ -z "${qAsmBuild}" ]; then
         printf "ERROR: can not find GenArk build directory for '%s'\n" "${query}" 1>&2
         exit 255
       fi
       qAsmName=`basename "${qAsmBuild}"`
       if [ "${query}" != "${qId}" -a "${query}" != "${qAsmName}" ]; then
         printf "ERROR: given query '%s' does not match build directory '%s'\n" "${query}" "${qAsmBuild}" 1>&2
         exit 255
       fi
       qRoot="${qAsmBuild}/trackData"
       query2bit="${qAsmBuild}/${qAsmName}.2bit"
       querySizes="${qAsmBuild}/${qAsmName}.chrom.sizes"
       qTdb="${qAsmBuild}/doTrackDb.bash"
       ;;
esac

printf "# tq: '${target}' '${query}'\n" 1>&2
printf "# tq id: '${tId}' '${qId}'\n" 1>&2
printf "# tq root: '${tRoot}' '${qRoot}'\n" 1>&2

### verify inputs are available

export inputErrors=0
for d in "${tRoot}" "${qRoot}"; do
  if [ ! -d "${d}" ]; then
    printf "ERROR: can not find directory ${d}\n" 1>&2
    inputErrors=$((inputErrors+1))
  fi
done
for f in "${target2bit}" "${query2bit}" "${targetSizes}" "${querySizes}"; do
  if [ ! -s "${f}" ]; then
    printf "ERROR: can not find ${f}\n" 1>&2
    inputErrors=$((inputErrors+1))
  fi
done
if [ ! -x "${doMiniMap2}" ]; then
  printf "ERROR: can not find ${doMiniMap2}\n" 1>&2
  inputErrors=$((inputErrors+1))
fi
for cmd in minimap2 pafToPsl twoBitToFa axtChain chainNet netChainSubset; do
  if ! command -v ${cmd} > /dev/null 2>&1; then
    printf "ERROR: can not find command '${cmd}' in PATH\n" 1>&2
    inputErrors=$((inputErrors+1))
  fi
done
if [ "${inputErrors}" -gt 0 ]; then
  exit 255
fi
if ! command -v mash > /dev/null 2>&1; then
  printf "# WARNING: can not find 'mash' in PATH, doMiniMap2.pl will fall back to -minimapPreset=asm5\n" 1>&2
fi

### determine what is already done, doMiniMap2.pl leaves a dateless
### symlink to the build directory, the fb.*Link.txt file from the
### load step indicates the run made it all the way through

export DS=`date "+%F"`
export buildDir="${tRoot}/miniMap2${Query}.${DS}"
export symLink="${tRoot}/miniMap2.${qId}"
export swapDir="${qRoot}/miniMap2.${tId}.swap"
export swapLink="${qRoot}/miniMap2.${tId}"

export primaryDone=0
export swapDone=0

if [ -L "${symLink}" ]; then
  linkDir=`readlink -f "${symLink}"`
  if [ -s "${linkDir}/fb.${tId}.chain${Query}Link.txt" ]; then
    printf "# ${query} -> ${target} already done: ${linkDir}\n" 1>&2
    primaryDone=1
    buildDir="${linkDir}"
  fi
fi

if [ -L "${swapLink}" ]; then
  linkDir=`readlink -f "${swapLink}"`
  if [ -s "${linkDir}/fb.${qId}.chain${Target}Link.txt" ]; then
    printf "# swap ${query} -> ${target} already done: ${linkDir}\n" 1>&2
    swapDone=1
    swapDir="${linkDir}"
  fi
fi

# directions completed by an earlier run do not go through doMiniMap2.pl
# again below, so -liftTableLoad has to be applied to them here
if [ -n "${liftTableLoad}" ]; then
  if [ "${primaryDone}" -eq 1 ]; then
    otherDir=""
    if [ "${swapDone}" -eq 1 ]; then
      otherDir="${swapDir}"
    fi
    liftTableRegister "${buildDir}" "${otherDir}" "${tId}" "${qId}"
  fi
  if [ "${swapDone}" -eq 1 ]; then
    otherDir=""
    if [ "${primaryDone}" -eq 1 ]; then
      otherDir="${buildDir}"
    fi
    # the swap direction is normally a *.swap directory, redone as the
    # -swap run from the primary build
    liftTableRegister "${swapDir}" "${otherDir}" "${qId}" "${tId}"
  fi
fi

if [ "${primaryDone}" -eq 1 -a "${swapDone}" -eq 1 ]; then
  printf "# all done\n" 1>&2
  if [ -s "${buildDir}/makeDoc.txt" ]; then
    cat "${buildDir}/makeDoc.txt"
  fi
  exit 0
fi

# a dated build directory that did not finish is either running right
# now or failed part way, either way it needs attention, do not start
# another one on top of it
if [ "${primaryDone}" -eq 0 ]; then
  export working=`ls -d ${tRoot}/miniMap2${Query}.* 2> /dev/null | wc -l`
  if [ "${working}" -gt 0 ]; then
    printf "# ERROR: in progress or failed, ${query} -> ${target}:\n" 1>&2
    ls -ogd ${tRoot}/miniMap2${Query}.* | sed -e 's/^/# /;' 1>&2
    printf "# check the do.log there, and finish it with, for example:\n" 1>&2
    printf "#   ${doMiniMap2} -buildDir=<dir> ${tId} ${qId}\n" 1>&2
    printf "# which starts at the first step that is not done yet\n" 1>&2
    printf "# or remove it to start over\n" 1>&2
    exit 255
  fi
fi

if [ "${swapDone}" -eq 0 -a -d "${swapDir}" ]; then
  printf "# ERROR: swap in progress or failed, ${query} -> ${target}:\n" 1>&2
  ls -ogd "${swapDir}" | sed -e 's/^/# /;' 1>&2
  printf "# check ${swapDir}/swap.log, doMiniMap2.pl -swap can be run again\n" 1>&2
  printf "# to resume it: ${swapDir}/runSwap.sh\n" 1>&2
  exit 255
fi

export tOrgName="$(orgName $tAsmName)"
export qOrgName="$(orgName $qAsmName)"
export tOrgDate="$(orgDate $tAsmName)"
export qOrgDate="$(orgDate $qAsmName)"
export tAsmSize=`awk '{sum+=$2}END{printf "%d", sum}' ${targetSizes}`
export qAsmSize=`awk '{sum+=$2}END{printf "%d", sum}' ${querySizes}`
export tSequenceCount=`wc -l < ${targetSizes}`
export qSequenceCount=`wc -l < ${querySizes}`
printf "# working: %s\n" "${buildDir}" 1>&2
printf "# target: $target - $tOrgName - $tOrgDate - $tSequenceCount sequences\n" 1>&2
printf "#  query: $query - $qOrgName - $qOrgDate - $qSequenceCount sequences\n" 1>&2
LC_NUMERIC=en_US printf "#  sizes: target: %'d - query: %'d\n" "${tAsmSize}" "${qAsmSize}" 1>&2

export doArgs="${liftTableLoad} -verbose=2 -workhorse=${workHorse} \\
  -bigClusterHub=${bigHub} -fileServer=${fileServer}"

### primary alignment, skip if it is already done
if [ "${primaryDone}" -eq 0 ]; then
  mkdir "${buildDir}"

### setup the buildDir/run.sh script
printf "#!/bin/bash

set -beEu -o pipefail

cd ${buildDir}

${doMiniMap2} ${doArgs} \\
  -buildDir=\`pwd\` ${tId} ${qId}
" > ${buildDir}/run.sh
chmod +x ${buildDir}/run.sh

### run the primary alignment
printf "running: time (${buildDir}/run.sh) >> ${buildDir}/do.log 2>&1\n" 1>&2

if ! { time ${buildDir}/run.sh ; } >> ${buildDir}/do.log 2>&1; then
  printf "ERROR: primary alignment failed, see ${buildDir}/do.log\n" 1>&2
  exit 255
fi

# rebuild trackDb if possible here
if [ -x "${tTdb}" ]; then
   ${tTdb}
else
   printf "# do not find tTdb '%s'\n" "${tTdb}" 1>&2
fi

fi      ###     if [ "${primaryDone}" -eq 0 ]; then

#### print out the makeDoc.txt to this point into buildDir/makeDoc.txt

if [ "${primaryDone}" -eq 0 ]; then
printf "##############################################################################
# miniMap2 ${tOrgName} ${Target} vs. ${qOrgName} ${Query}
#    (DONE - $DS - $userName)

    mkdir $buildDir
    cd $buildDir

    time (${doMiniMap2} ${doArgs} \\
      -buildDir=\`pwd\` ${tId} ${qId}) >> do.log 2>&1
    grep -w real do.log | sed -e 's/^/    # /;'
" > ${buildDir}/makeDoc.txt

(grep -w real ${buildDir}/do.log || true) | tail -1 | sed -e 's/^/    # /;' >> ${buildDir}/makeDoc.txt

printf "\n    cat version.txt\n" >> ${buildDir}/makeDoc.txt
fbLines ${buildDir}/version.txt >> ${buildDir}/makeDoc.txt

printf "\n    sed -e 's/^/    # /;' fb.${tId}.chain${Query}Link.txt\n" >> ${buildDir}/makeDoc.txt
fbLines ${buildDir}/fb.${tId}.chain${Query}Link.txt >> ${buildDir}/makeDoc.txt

printf "    sed -e 's/^/    # /;' fb.${tId}.chainLiftOver${Query}.txt\n" >> ${buildDir}/makeDoc.txt
fbLines ${buildDir}/fb.${tId}.chainLiftOver${Query}.txt >> ${buildDir}/makeDoc.txt

printf "\n    ### and for the swap\n" >> ${buildDir}/makeDoc.txt
fi

if [ -s "${buildDir}/makeDoc.txt" ]; then
  cat ${buildDir}/makeDoc.txt
fi

printf "# swap into: ${swapDir}\n" 1>&2

### swap alignment, skip if it is already done
if [ "${swapDone}" -eq 0 ]; then
mkdir -p ${swapDir}

printf "#!/bin/bash

set -beEu -o pipefail

cd ${swapDir}

${doMiniMap2} -swap ${doArgs} \\
  -buildDir=${buildDir} -swapDir=\`pwd\` ${tId} ${qId}
" > ${swapDir}/runSwap.sh
chmod +x ${swapDir}/runSwap.sh

printf "# running ${swapDir}/runSwap.sh\n" 1>&2

if ! { time ${swapDir}/runSwap.sh ; } >> ${swapDir}/swap.log 2>&1; then
  printf "ERROR: swap failed, see ${swapDir}/swap.log\n" 1>&2
  exit 255
fi

# rebuild trackDb if possible here
if [ -x "${qTdb}" ]; then
   ${qTdb}
else
   printf "# do not find qTdb '%s'\n" "${qTdb}" 1>&2
fi

### continue the make doc

printf "\n    cd ${swapDir}\n" >> ${buildDir}/makeDoc.txt

printf "\n    time (${doMiniMap2} -swap ${doArgs} \\
      -buildDir=${buildDir} -swapDir=\`pwd\` ${tId} ${qId}) >> swap.log 2>&1
    grep -w real swap.log | sed -e 's/^/    # /;'
" >> ${buildDir}/makeDoc.txt

(grep -w real ${swapDir}/swap.log || true) | tail -1 | sed -e 's/^/    # /;' >> ${buildDir}/makeDoc.txt

printf "\n    sed -e 's/^/    # /;' fb.${qId}.chain${Target}Link.txt\n" >> ${buildDir}/makeDoc.txt
fbLines ${swapDir}/fb.${qId}.chain${Target}Link.txt >> ${buildDir}/makeDoc.txt

printf "    sed -e 's/^/    # /;' fb.${qId}.chainLiftOver${Target}.txt\n" >> ${buildDir}/makeDoc.txt
fbLines ${swapDir}/fb.${qId}.chainLiftOver${Target}.txt >> ${buildDir}/makeDoc.txt

printf "\n##############################################################################\n" >> ${buildDir}/makeDoc.txt

fi      ### if [ "${swapDone}" -eq 0 ]; then

### show completed makeDoc.txt ####
cat ${buildDir}/makeDoc.txt

### end seconds
export endT=`date "+%s"`

export toAddress="$userName"
export fromAddress="$userName"
export subject="pair miniMap2 DONE $target $query"
printf "To: $toAddress
From: $fromAddress
Subject: $subject

##################################################################
" > /tmp/send.txt.$$
date >> /tmp/send.txt.$$
printf "##################################################################\n" >> /tmp/send.txt.$$
cat ${buildDir}/makeDoc.txt >> /tmp/send.txt.$$

### show elapsed time
printf "%s\t%s\n" "${endT}" "${startT}" | awk -F$'\t' '{
seconds=$1-$2
hours=int(seconds/3600)
minutes=int((seconds-(hours*3600))/60)
s=seconds % 60
printf "### elapsed time: %02dh %02dm %02ds\n\n", hours, minutes, s
}' >> /tmp/send.txt.$$

date >> /tmp/send.txt.$$
printf "##################################################################\n" >> /tmp/send.txt.$$

cat /tmp/send.txt.$$ | /usr/sbin/sendmail -t -oi

rm -f /tmp/send.txt.$$

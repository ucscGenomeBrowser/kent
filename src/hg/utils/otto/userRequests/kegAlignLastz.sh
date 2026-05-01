#!/bin/bash

set -eEu -o pipefail

export userName="`whoami`"

export bigHub="hgwdev"
export workHorse="hgwdev"
export smallClusterHub="hgwdev"
export fileServer="hgwdev"
#  a python virtual environment to run this
export planemoCmd="/hive/users/hiram/galaxy/venv3.12/bin/planemo"

# parse optional -force flag to skip genark table verification
export forceRun=0
if [ $# -gt 0 ] && [ "$1" = "-force" ]; then
  forceRun=1
  shift
fi

if [ $# != 4 ]; then
  printf "ERROR: arg count: %d != 4\n" "$#" 1>&2

  printf "usage: kegAlignLastz.sh [-force] <target> <query> <tClade> <qClade>

Where target/query is either a UCSC db name, or is an
   assembly hub identifier, e.g.: GCA_002844635.1_USMARCv1.0.1

And [tq]Clade is one of: primate|mammal|other

The -force option skips the hgcentraltest.genark table verification
   for GenArk assembly identifiers.

Will create directory to work in, for example if, UCSC db:
  /hive/data/target/bed/lastzQuery.yyyy-mm-dd/

Or, in the assembly hub build directory:
/hive/data/genomes/asmHubs/allBuild/GCA/002/844/635/GCA_002844635.1_USMARCv1.0/trackData/lastzQuery.yyyy-mm-dd

Will set up a DEF file there, and a kegAlign.sh script to run all steps
  and output makeDoc text to document what happened.
  Also sets up a lastzRun.sh script that could be used to run the UCSC
  lastz procedure.

AND MORE, it will run the swap operation into the corresponding
  blastz.target.swap directory in the query genome work space.

Email will be sent to: '$userName' upon completion.

e.g.: kegAlignLastz.sh rn7 papAnu4 mammal primate\n" 1>&2
  exit 255
fi

# accId - if assembly hub, determine GCx_012345678.9 name
#         if not, return the asmName (== UCSC database name)
function accId() {
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

# asmSize - determine size of genome
function asmSize() {
  export asmName=$1
  export sizes="/hive/data/genomes/${asmName}/chrom.sizes"
  case $asmName in
     GC[AF]_*)
       gcxPath=$(gcPath $asmName)
       id=$(accId $asmName)
       size=`awk '{sum+=$2}END{print sum}' /hive/data/genomes/asmHubs/${gcxPath}/${id}/${id}.chrom.sizes.txt`
       ;;
     *)
       size=`awk '{sum+=$2}END{print sum}' ${sizes}`
       ;;
  esac
  printf "%s" "${size}"
}

# seqCount - determine the sequence count in given genome target
function seqCount() {
  export asmName=$1
  export sizes="/hive/data/genomes/${asmName}/chrom.sizes"
  case $asmName in
     GC[AF]_*)
       gcxPath=$(gcPath $asmName)
       id=$(accId $asmName)
       count=`wc -l /hive/data/genomes/asmHubs/${gcxPath}/${id}/${id}.chrom.sizes.txt | cut -d' ' -f1`
       ;;
     *)
       count=`wc -l ${sizes} | cut -d' ' -f1`
       ;;
  esac
  printf "%s" "${count}"
}

# obtain the organism name out of the assembly_report.txt file
function orgName() {
  export asmName=$1
  case $asmName in
     GC[AF]_*)
       gcxPath=$(gcPath $asmName)
       asmDir="/hive/data/outside/ncbi/genomes/${gcxPath}/${asmName}"
       asmRpt="${asmDir}/${asmName}_assembly_report.txt"
       oName=`egrep -m 1 -i "^# organism name:" ${asmRpt} | tr -d '\r' | sed -e 's/.*(//; s/).*//'`
       ;;
     *)
       oName=`hgsql -N -e "select organism from dbDb where name=\"${asmName}\";" hgcentraltest`
       ;;
  esac
  printf "%s" "${oName}"
}
 
# generate URL to the fa.gz files to pass off to galaxy
function faGzUrl() {
  export asmName=$1
  case $asmName in
     GC[AF]_*)
       gcxPath=$(gcPath $asmName)
       id=$(accId $asmName)
       printf "https://hgdownload.soe.ucsc.edu/hubs/%s/%s/%s.fa.gz" "${gcxPath}" "${id}" "${id}"
       ;;
     *)
       printf "https://hgdownload.soe.ucsc.edu/goldenPath/%s/bigZips/%s.fa.gz" "${asmName}" "${asmName}"
       ;;
  esac
}

# get the assembly date out of the assembly_report.txt file
function orgDate() {
  export asmName=$1
  case $asmName in
     GC[AF]_*)
       gcxPath=$(gcPath $asmName)
       asmDir="/hive/data/outside/ncbi/genomes/${gcxPath}/${asmName}"
       asmRpt="${asmDir}/${asmName}_assembly_report.txt"
       oDate=`egrep -m 1 -i "^#[[:space:]]*Date:" ${asmRpt} | tr -d '\r' | sed -e 's/.*ate: \+//;'`
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

##############################################################################
##############################################################################
### start seconds
export startT=`date "+%s"`

export target="$1"
export query="$2"
export tClade="$3"
export qClade="$4"

export tGcPath=$(gcPath $target)
export qGcPath=$(gcPath $query)
export tAccId=$(accId $target)
export qAccId=$(accId $query)

# verify GenArk assemblies exist in hgcentraltest.genark unless -force
if [ "$forceRun" -eq 0 ]; then
  export genarkErrors=0
  case $target in
    GC[AF]_*) verifyGenark "$tAccId" "$target" || genarkErrors=$((genarkErrors+1)) ;;
  esac
  case $query in
    GC[AF]_*) verifyGenark "$qAccId" "$query" || genarkErrors=$((genarkErrors+1)) ;;
  esac
  if [ "$genarkErrors" -gt 0 ]; then
    printf "Use -force to skip this check\n" 1>&2
    exit 255
  fi
fi

printf "# tq: '${target}' '${query}' '${tClade}' '${qClade}'\n" 1>&2
printf "# tq gcPath: '${tGcPath}' '${qGcPath}'\n" 1>&2
printf "# tq accId: '${tAccId}' '${qAccId}'\n" 1>&2

# upper case first character
export Target="${tAccId^}"
export Query="${qAccId^}"

export DS=`date "+%F"`

# assume UCSC db build
export buildDir="/hive/data/genomes/${target}/bed/lastz${Query}.${DS}"
export targetExists="/hive/data/genomes/${target}/bed"
export symLink="/hive/data/genomes/${target}/bed/lastz.${qAccId}"
export swapDir="/hive/data/genomes/${query}/bed/blastz.${tAccId}.swap"
export queryExists="/hive/data/genomes/${query}/bed"
export swapLink="/hive/data/genomes/${query}/bed/lastz.${tAccId}"
export targetSizes="/hive/data/genomes/${target}/chrom.sizes"
export querySizes="/hive/data/genomes/${query}/chrom.sizes"
export target2bit="/hive/data/genomes/${target}/${target}.2bit"
export query2bit="/hive/data/genomes/${query}/${query}.2bit"
export trackHub=""
export rBestTrackHub=""
export tRbestArgs=""
export qRbestArgs=""
export tSwapRbestArgs=""
export qSwapRbestArgs=""
export tFullName=""
export qFullName=""
export tTdb="xxx"
export qTdb="xxx"

#  override those specifications if assembly hub
case $target in
     GC[AF]_*) trackHub="-trackHub -noDbNameCheck"
       tFullName="-tAsmId $target"
       rBestTrackHub="-trackHub"
       buildDir="/hive/data/genomes/asmHubs/allBuild/${tGcPath}/${target}/trackData/lastz${Query}.${DS}"
       symLink="/hive/data/genomes/asmHubs/allBuild/${tGcPath}/${target}/trackData/lastz.${qAccId}"
       targetExists="/hive/data/genomes/asmHubs/allBuild/${tGcPath}/${target}/trackData"
       targetSizes="/hive/data/genomes/asmHubs/${tGcPath}/${tAccId}/${tAccId}.chrom.sizes.txt"
       target2bit="/hive/data/genomes/asmHubs/${tGcPath}/${tAccId}/${tAccId}.2bit"
       tTdb="/hive/data/genomes/asmHubs/allBuild/${tGcPath}/${target}/doTrackDb.bash"
       tRbestArgs="-target2Bit=\"${target2bit}\" \\
-targetSizes=\"${targetSizes}\""
       tSwapRbestArgs="-query2bit=\"${target2bit}\" \\
-querySizes=\"${targetSizes}\""
       ;;
esac

case $query in
     GC[AF]_*) trackHub="-trackHub -noDbNameCheck"
       qFullName="-qAsmId $query"
       rBestTrackHub="-trackHub"
       swapDir="/hive/data/genomes/asmHubs/allBuild/${qGcPath}/${query}/trackData/blastz.${tAccId}.swap"
       swapLink="/hive/data/genomes/asmHubs/allBuild/${qGcPath}/${query}/trackData/lastz.${tAccId}"
       queryExists="/hive/data/genomes/asmHubs/allBuild/${qGcPath}/${query}/trackData"
       querySizes="/hive/data/genomes/asmHubs/${qGcPath}/${qAccId}/${qAccId}.chrom.sizes.txt"
       query2bit="/hive/data/genomes/asmHubs/${qGcPath}/${qAccId}/${qAccId}.2bit"
       qTdb="/hive/data/genomes/asmHubs/allBuild/${qGcPath}/${query}/doTrackDb.bash"
       qRbestArgs="-query2Bit=\"${query2bit}\" \\
-querySizes=\"${querySizes}\""
       qSwapRbestArgs="-target2bit=\"${query2bit}\" \\
-targetSizes=\"${querySizes}\""
       ;;
esac


if [ ! -d "${targetExists}" ]; then
  printf "ERROR: can not find ${targetExists}\n" 1>&2
  exit 255
fi

if [ ! -d "${queryExists}" ]; then
  printf "ERROR: can not find ${queryExists}\n" 1>&2
  exit 255
fi

if [ ! -s "${target2bit}" ]; then
  printf "ERROR: can not find ${target2bit}\n" 1>&2
  exit 255
fi

if [ ! -s "${query2bit}" ]; then
  printf "ERROR: can not find ${query2bit}\n" 1>&2
  exit 255
fi


if [ ! -s "${targetSizes}" ]; then
  printf "ERROR: can not find ${targetSizes}\n" 1>&2
  exit 255
fi

if [ ! -s "${querySizes}" ]; then
  printf "ERROR: can not find ${querySizes}\n" 1>&2
  exit 255
fi

export doneCount=0
export primaryDone=0
export swapDone=0

if [ -L "${symLink}" ]; then
  printf "# ${query} -> ${target} already done\n" 1>&2
  doneCount=`echo $doneCount | awk '{printf "%d", $1+1}'`
  primaryDone=1
else
  printf "# primaryDone $primaryDone no symLink: $symLink\n" 1>&2
fi

if [ -L "${swapLink}" ]; then
  printf "# swap ${query} -> ${target} already done\n" 1>&2
  doneCount=`echo $doneCount | awk '{printf "%d", $1+1}'`
  swapDone=1
fi

export working=`ls -d ${targetExists}/lastz${Query}.* 2> /dev/null | wc -l`
if [ "${working}" -gt 0 ]; then
  if [ "${primaryDone}" -eq 0 ]; then
    printf "# in progress, ${query} -> ${target}:\n" 1>&2
    printf "# " 1>&2
    ls -ogd ${targetExists}/lastz${Query}.* 1>&2
  fi
  buildDir=`ls -d ${targetExists}/lastz${Query}.* | tail -1`
  printf "# continuing: %s\n" "${buildDir}" 1>&2
fi
export primaryPartsDone=`ls $buildDir/fb.* 2> /dev/null | wc -l`
if [ "$primaryPartsDone" -gt 0 ]; then
  primaryDone="$primaryPartsDone"
  printf "# primaryPartsDone $primaryPartsDone primaryDone $primaryDone\n" 1>&2
fi

if [ -d "${swapDir}" ]; then
  printf "# swap in progress ${query} -> ${target}\n" 1>&2
  printf "# " 1>&2
  ls -ogd "${swapDir}" 1>&2
  if [ "${doneCount}" -ne 2 ]; then
    printf "# doneCount $doneCount -ne 2 exit 0 since swap in progress\n" 1>&2
    exit 0
  fi
fi

if [ "${doneCount}" -eq 2 ]; then
    printf "# all done\n" 1>&2
fi

export tOrgName="$(orgName $target)"
export qOrgName="$(orgName $query)"
export tOrgDate="$(orgDate $target)"
export qOrgDate="$(orgDate $query)"
export tAsmSize="$(asmSize $target)"
export qAsmSize="$(asmSize $query)"
export tSequenceCount="$(seqCount $target)"
export qSequenceCount="$(seqCount $query)"
export tFaGzUrl="$(faGzUrl $target)"
export qFaGzUrl="$(faGzUrl $query)"
printf "# working: %s\n" "${buildDir}" 1>&2
printf "# target: $target - $tOrgName - $tOrgDate - $tClade - $tSequenceCount sequences\n" 1>&2
printf "#  query: $query - $qOrgName - $qOrgDate - $qClade - $qSequenceCount sequences\n" 1>&2
LC_NUMERIC=en_US printf "#  sizes: target: %'d - query: %'d\n" "${tAsmSize}" "${qAsmSize}" 1>&2

export seq1Limit="40"
if [ "${tSequenceCount}" -gt 50000 ]; then
  seq1Limit="100"
fi

export seq2Limit="100"
if [ "${qSequenceCount}" -gt 50000 ]; then
  seq2Limit="500"
fi

export minScore="3000"
export linearGap="medium"

case $tClade in
   "primate")
     case $qClade in
        "primate")
           minScore="5000"
           ;;
        "mammal")
           ;;
        "other")
           minScore="5000"
           linearGap="loose"
           ;;
     esac
     ;;
   "mammal")
     case $qClade in
        "primate")
           ;;
        "mammal")
           ;;
        "other")
           minScore="5000"
           linearGap="loose"
           ;;
     esac
     ;;
   "other")
      minScore="5000"
      linearGap="loose"
esac

##########  primate <-> primate  #########################################
if [ "$tClade" == "primate" -a "$qClade" == "primate" ]; then

export yamlString="# ${tAccId}.${qAccId}.yaml
TARGET_Sequence:
  class: File
  path: ${tFaGzUrl}
QUERY_Sequence:
  class: File
  path: ${qFaGzUrl}
# axtChain options
axtChainMinScore: ${minScore}
axtChainLinearGap:
  class: File
  filetype: txt
  path: https://genome-test.gi.ucsc.edu/~hiram/galaxy/kegAlign/axtChain.medium.txt
axtChainScoreScheme:
  class: File
  filetype: txt
  path: https://genome-test.gi.ucsc.edu/~hiram/galaxy/kegAlign/primate-primate.lastz.score.txt
#    A    C    G    T
#   90 -330 -236 -356
# -330  100 -318 -236
# -236 -318  100 -330
# -356 -236 -330   90
#     O=600 E=150
# lastz options
xdropX: 910
ydropY: 15000
stepZ: 1
noTransitionT: false
strand_selectorB: both
# seeding_options.seed.seed_selector: 12of19 does not get into the process
hspthreshK: 4500
gappedthreshL: 4500
innerH: 2000
scoringMatrix:
  class: File
  filetype: txt
  path: https://genome-test.gi.ucsc.edu/~hiram/galaxy/kegAlign/primate-primate.lastz.score.txt
"

export defString="# ${qOrgName} ${Query} vs. ${tOrgName} ${Target}
BLASTZ=/cluster/bin/penn/lastz-distrib-1.04.03/bin/lastz
BLASTZ_T=2
BLASTZ_O=600
BLASTZ_E=150
BLASTZ_M=254
BLASTZ_K=4500
BLASTZ_Y=15000
BLASTZ_Q=/hive/data/staging/data/blastz/human_chimp.v2.q
#       A     C     G     T
# A    90  -330  -236  -356
# C  -330   100  -318  -236
# G  -236  -318   100  -330
# T  -356  -236  -330    90

# TARGET: ${tOrgName} ${tOrgDate} ${target}
SEQ1_DIR=${target2bit}
SEQ1_LEN=${targetSizes}
SEQ1_CHUNK=20000000
SEQ1_LAP=10000
SEQ1_LIMIT=${seq1Limit}

# QUERY: ${qOrgName} ${qOrgDate} ${query}
SEQ2_DIR=${query2bit}
SEQ2_LEN=${querySizes}
SEQ2_CHUNK=20000000
SEQ2_LAP=0
SEQ2_LIMIT=${seq2Limit}

BASE=${buildDir}
TMPDIR=/dev/shm
"
else

export yamlString="# ${target}.${query}.yaml
TARGET_Sequence:
  class: File
  path: ${tFaGzUrl}
QUERY_Sequence:
  class: File
  path: ${qFaGzUrl}
# axtChain options
axtChainMinScore: ${minScore}
axtChainLinearGap:
  class: File
  filetype: txt
  path: https://genome-test.gi.ucsc.edu/~hiram/galaxy/kegAlign/axtChain.loose.txt
# lastz options
xdropX: 910
ydropY: 9400
stepZ: 1
noTransitionT: true
strand_selectorB: both
# seeding_options.seed.seed_selector: 12of19 does not get into the process
hspthreshK: 3000
gappedthreshL: 3000
innerH: 2000
"
export defString="# ${qOrgName} ${Query} vs. ${tOrgName} ${Target}
BLASTZ=/cluster/bin/penn/lastz-distrib-1.04.03/bin/lastz

# TARGET: ${tOrgName} ${tOrgDate} ${target}
SEQ1_DIR=${target2bit}
SEQ1_LEN=${targetSizes}
SEQ1_CHUNK=20000000
SEQ1_LAP=10000
SEQ1_LIMIT=${seq1Limit}

# QUERY: ${qOrgName} ${qOrgDate} ${query}
SEQ2_DIR=${query2bit}
SEQ2_LEN=${querySizes}
SEQ2_CHUNK=20000000
SEQ2_LAP=0
SEQ2_LIMIT=${seq2Limit}

BASE=${buildDir}
TMPDIR=/dev/shm
"

fi

### skip primary alignment if it is already done
###  primaryDone == 0 means NOT done yet
if [ $primaryDone -eq 0 ]; then
  mkdir -p "${buildDir}"

### setup the DEF file
printf "%s" "${defString}" > ${buildDir}/DEF
### and the yaml file
printf "%s" "${yamlString}" > ${buildDir}/${tAccId}.${qAccId}.yaml

### setup the buildDir/lastzRun.sh script
printf "#!/bin/bash

set -eEu -o pipefail

export targetDb=\"${tAccId}\"
export queryDb=\"${qAccId}\"
export TargetDb=\"${Target}\"
export QueryDb=\"${Query}\"

cd ${buildDir}
time (~/kent/src/hg/utils/automation/doBlastzChainNet.pl ${trackHub} -verbose=2 \`pwd\`/DEF -syntenicNet \\
  $tFullName $qFullName -workhorse=${workHorse} -smallClusterHub=${smallClusterHub} \\
    -bigClusterHub=${bigHub} \\
    -chainMinScore=${minScore} -chainLinearGap=${linearGap}) > do.log 2>&1

grep -w real do.log | sed -e 's/^/    # /;'

sed -e 's/^/    # /;' fb.\${targetDb}.chain\${QueryDb}Link.txt
sed -e 's/^/    # /;' fb.\${targetDb}.chainSyn\${QueryDb}Link.txt

time (~/kent/src/hg/utils/automation/doRecipBest.pl ${rBestTrackHub} -load -workhorse=${workHorse} -buildDir=\`pwd\` \\
   ${tRbestArgs} \\
   ${qRbestArgs} \\
   \${targetDb} \${queryDb}) > rbest.log 2>&1

grep -w real rbest.log | sed -e 's/^/    # /;'

sed -e 's/^/    #/;' fb.\${targetDb}.chainRBest.\${QueryDb}.txt
" > ${buildDir}/lastzRun.sh
chmod +x ${buildDir}/lastzRun.sh

### setup the buildDir/kegAlign.sh script
printf "#!/bin/bash

# exit on any failure
set -eEu -o pipefail

export buildDir=\"${buildDir}\"
export swapDir=\"${swapDir}\"
export PM=\"${planemoCmd}\"

export targetDb=\"${tAccId}\"
export queryDb=\"${qAccId}\"
export QueryDb=\"${Query}\"
export Target=\"${Target}\"
export tSizes=\"${targetSizes}\"
export qSizes=\"${querySizes}\"

############################################################################
# chainBigBedFb - convert chain to bigBed and compute featureBits
# args: db chainName chainGz sizesFile fbFile
function chainBigBedFb() {
  local db=\$1
  local chainName=\$2
  local chainGz=\$3
  local sizesFile=\$4
  local fbFile=\$5
  chainToBigChain \"\${chainGz}\" \${chainName}.tab \${chainName}Link.tab
  bedToBigBed -type=bed6+6 -as=\$HOME/kent/src/hg/lib/bigChain.as -tab \${chainName}.tab \${sizesFile} \${chainName}.bb
  bedToBigBed -type=bed4+1 -as=\$HOME/kent/src/hg/lib/bigLink.as -tab \${chainName}Link.tab \${sizesFile} \${chainName}Link.bb
  rm -f \${chainName}.tab \${chainName}Link.tab chain.tab link.tab
  local totalBases=\`ave -col=2 \${sizesFile} | grep \"^total\" | awk '{printf \"%%d\", \$2}'\`
  local basesCovered=\`bigBedInfo \${chainName}Link.bb | grep \"basesCovered\" | cut -d' ' -f2 | tr -d ','\`
  local percentCovered=\`echo \${basesCovered} \${totalBases} | awk '{printf \"%%.3f\", 100.0*\$1/\$2}'\`
  printf \"%%d bases of %%d (%%s%%) in intersection\\\n\" \"\${basesCovered}\" \"\${totalBases}\" \"\${percentCovered}\" > \${fbFile}
}
############################################################################

cd \${buildDir}
mkdir -p log
export DS=\`date \"+%%F_%%T_%%s\"\`
if [ -s pendingInvocationId.txt ]; then
  DS=\`cut -f1 pendingInvocationId.txt\`
fi
export logDir=\"\${buildDir}/log\"
export logFile=\"\${logDir}/\${DS}.log\"
### only try to submit if the workflow has not already been submitted
if [ ! -s \"pendingInvocationId.txt\" ]; then
  \${PM} run \\
    ~/kent/src/hg/utils/automation/kegAlign.json.ga \\
       \"\${targetDb}.\${queryDb}.yaml\" --profile vgp \\
          --history_name \"\${targetDb}.\${queryDb}.kegAlign\" \\
             --no_wait \\
             --test_output_json \"\${logDir}/runOutput.\${DS}.json\" \\
                >> \"\${logFile}\" 2>&1

  invocationId=\`jq -r '.tests[0].data.invocation_details.details.invocation_id' \"\${logDir}/runOutput.\${DS}.json\"\`
  printf \"invocation ID: %%s\\n\" \"\${invocationId}\" 1>&2
  # record the invocation ID and associated log file for monitoring cron
  printf \"%%s\\t%%s\\t%%s\\n\" \"\${DS}\" \"\${invocationId}\" \"\${logDir}/runOutput.\${DS}.json\" > pendingInvocationId.txt
fi

printf \"### workflow submitted, invocation ID in: %%s/pendingInvocationId.txt\\n\" \"\${buildDir}\" 1>&2

" > ${buildDir}/kegAlign.sh
chmod +x ${buildDir}/kegAlign.sh

### run the primary alignment, galaxy kegAlign style
printf "running: time (${buildDir}/kegAlign.sh) >> ${buildDir}/kegAlign.log 2>&1\n" 1>&2

time (${buildDir}/kegAlign.sh) >> ${buildDir}/keg.log 2>&1

fi	#	if [ $primaryDone -eq 0 ]; then

exit $?

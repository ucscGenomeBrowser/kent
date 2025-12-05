#!/bin/bash

set -beEu -o pipefail

export userName="`whoami`"

export bigHub="hgwdev"
export workHorse="hgwdev"
export smallClusterHub="hgwdev"
export fileServer="hgwdev"

if [ $# != 4 ]; then
  printf "ERROR: arg count: %d != 4\n" "$#" 1>&2

  printf "usage: pairLastz.sh <target> <query> <tClade> <qClade>

Where target/query is either a UCSC db name, or is an
   assembly hub identifier, e.g.: GCA_002844635.1_USMARCv1.0.1

And [tq]Clade is one of: primate|mammal|other

Will create directory to work in, for example if, UCSC db:
  /hive/data/target/bed/lastzQuery.yyyy-mm-dd/

Or, in the assembly hub build directory:
/hive/data/genomes/asmHubs/allBuild/GCA/002/844/635/GCA_002844635.1_USMARCv1.0/trackData/lastzQuery.yyyy-mm-dd

Will set up a DEF file there, and a run.sh script to run all steps
  and output makeDoc text to document what happened.

AND MORE, it will run the swap operation into the corresponding
  blastz.target.swap directory in the query genome work space.

Email will be sent to: '$userName' upon completion.

e.g.: pairLastz.sh rn7 papAnu4 mammal primate\n" 1>&2
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

# asmSize - determine size of genome
function asmSize() {
  export asmName=$1
  export sizes="/hive/data/genomes/${asmName}/chrom.sizes"
  case $asmName in
     GC[AF]_*)
       gcxPath=$(gcPath $asmName)
       id=$(asmId $asmName)
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
       id=$(asmId $asmName)
       count=`wc -l /hive/data/genomes/asmHubs/${gcxPath}/${id}/${id}.chrom.sizes.txt | cut -d' ' -f1`
       ;;
     *)
       count=`wc -l ${sizes} | cut -d' ' -f1`
       ;;
  esac
  printf "%s" "${count}"
}

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

# check if this database is actually a database browser or a promoted
# hub.  It could have a MySQL database, but it won't have a chromInfo
function promotedHub() {
  export db=$1
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
export tAsmId=$(asmId $target)
export qAsmId=$(asmId $query)
printf "# tq: '${target}' '${query}' '${tClade}' '${qClade}'\n" 1>&2
printf "# tq gcPath: '${tGcPath}' '${qGcPath}'\n" 1>&2
printf "# tq asmId: '${tAsmId}' '${qAsmId}'\n" 1>&2

# upper case first character
export Target="${tAsmId^}"
export Query="${qAsmId^}"

export DS=`date "+%F"`

# assume UCSC db build
export buildDir="/hive/data/genomes/${target}/bed/lastz${Query}.${DS}"
export targetExists="/hive/data/genomes/${target}/bed"
export symLink="/hive/data/genomes/${target}/bed/lastz.${qAsmId}"
export swapDir="/hive/data/genomes/${query}/bed/blastz.${tAsmId}.swap"
export queryExists="/hive/data/genomes/${query}/bed"
export swapLink="/hive/data/genomes/${query}/bed/lastz.${tAsmId}"
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
       symLink="/hive/data/genomes/asmHubs/allBuild/${tGcPath}/${target}/trackData/lastz.${qAsmId}"
       targetExists="/hive/data/genomes/asmHubs/allBuild/${tGcPath}/${target}/trackData"
       targetSizes="/hive/data/genomes/asmHubs/${tGcPath}/${tAsmId}/${tAsmId}.chrom.sizes.txt"
       target2bit="/hive/data/genomes/asmHubs/${tGcPath}/${tAsmId}/${tAsmId}.2bit"
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
       swapDir="/hive/data/genomes/asmHubs/allBuild/${qGcPath}/${query}/trackData/blastz.${tAsmId}.swap"
       swapLink="/hive/data/genomes/asmHubs/allBuild/${qGcPath}/${query}/trackData/lastz.${tAsmId}"
       queryExists="/hive/data/genomes/asmHubs/allBuild/${qGcPath}/${query}/trackData"
       querySizes="/hive/data/genomes/asmHubs/${qGcPath}/${qAsmId}/${qAsmId}.chrom.sizes.txt"
       query2bit="/hive/data/genomes/asmHubs/${qGcPath}/${qAsmId}/${qAsmId}.2bit"
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

if [ "$tClade" == "primate" -a "$qClade" == "primate" ]; then

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
  mkdir "${buildDir}"

### setup the DEF file
printf "%s" "${defString}" > ${buildDir}/DEF

### setup the buildDir/run.sh script
printf "#!/bin/bash

set -beEu -o pipefail

export targetDb=\"${tAsmId}\"
export queryDb=\"${qAsmId}\"
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
" > ${buildDir}/run.sh
chmod +x ${buildDir}/run.sh

### run the primary alignment
printf "running: time (${buildDir}/run.sh) >> ${buildDir}/do.log 2>&1\n" 1>&2

time (${buildDir}/run.sh) >> ${buildDir}/do.log 2>&1

# rebuild trackDb if possible here
if [ -x "${tTdb}" ]; then
   ${tTdb}
else
   printf "# do not find tTdb '%s'\n" "${tTdb}" 1>&2
fi

fi      ###     if [ $primaryDone -eq 0 ]; then

#### print out the makeDoc.txt to this point into buildDir/makeDoc.txt

printf "##############################################################################
# LASTZ ${tOrgName} ${Target} vs. $qOrgName ${Query}
#    (DONE - $DS - $userName)

    mkdir $buildDir
    cd $buildDir

    printf '${defString}
' > DEF

    time (~/kent/src/hg/utils/automation/doBlastzChainNet.pl ${trackHub} -verbose=2 \`pwd\`/DEF -syntenicNet \\
      ${tFullName} ${qFullName} -workhorse=${workHorse} -smallClusterHub=${smallClusterHub} -fileServer=${fileServer} -bigClusterHub=${bigHub} \\
        -chainMinScore=${minScore} -chainLinearGap=${linearGap}) > do.log 2>&1
    grep -w real do.log | sed -e 's/^/    # /;'
" > ${buildDir}/makeDoc.txt

(grep -w real $buildDir/do.log || true) | sed -e 's/^/    # /;' | head -1 >> ${buildDir}/makeDoc.txt

printf "\n    sed -e 's/^/    # /;' fb.${tAsmId}.chain${Query}Link.txt\n" >> ${buildDir}/makeDoc.txt
sed -e 's/^/    # /;' $buildDir/fb.${tAsmId}.chain${Query}Link.txt >> ${buildDir}/makeDoc.txt

printf "    sed -e 's/^/    # /;' fb.${tAsmId}.chainSyn${Query}Link.txt\n" >> ${buildDir}/makeDoc.txt
sed -e 's/^/    # /;' $buildDir/fb.${tAsmId}.chainSyn${Query}Link.txt >> ${buildDir}/makeDoc.txt

printf "\n    time (~/kent/src/hg/utils/automation/doRecipBest.pl ${rBestTrackHub} -load -workhorse=${workHorse} -buildDir=\`pwd\` \\
      ${tRbestArgs} \\
      ${qRbestArgs} \\
        ${tAsmId} ${qAsmId}) > rbest.log 2>&1

    grep -w real rbest.log | sed -e 's/^/    # /;'\n" >> ${buildDir}/makeDoc.txt

(grep -w real $buildDir/rbest.log || true) | sed -e 's/^/    # /;' >> ${buildDir}/makeDoc.txt

printf "\n    sed -e 's/^/    # /;' fb.${tAsmId}.chainRBest.${Query}.txt\n" >> ${buildDir}/makeDoc.txt
(sed -e 's/^/    # /;' ${buildDir}/fb.${tAsmId}.chainRBest.${Query}.txt || true) >> ${buildDir}/makeDoc.txt

printf "\n    ### and for the swap\n" >> ${buildDir}/makeDoc.txt

cat ${buildDir}/makeDoc.txt

printf "# swap into: ${swapDir}\n" 1>&2

if [ "$swapDone" -eq 0 ]; then
mkdir ${swapDir}

ln -s ${buildDir}/DEF ${swapDir}/DEF

printf "#!/bin/bash

set -beEu -o pipefail

cd $swapDir

export targetDb=\"${tAsmId}\"
export Target=\"${Target}\"
export queryDb=\"${qAsmId}\"

time (~/kent/src/hg/utils/automation/doBlastzChainNet.pl ${trackHub}  -swap -verbose=2 \\
  ${tFullName} ${qFullName} ${buildDir}/DEF -swapDir=\`pwd\` \\
  -syntenicNet -workhorse=${workHorse} -smallClusterHub=${smallClusterHub} -fileServer=${fileServer} -bigClusterHub=${bigHub} \\
    -chainMinScore=${minScore} -chainLinearGap=${linearGap}) > swap.log 2>&1

grep -w real swap.log | sed -e 's/^/    # /;'

sed -e 's/^/    # /;' fb.\${queryDb}.chain\${Target}Link.txt
sed -e 's/^/    # /;' fb.\${queryDb}.chainSyn\${Target}Link.txt
time (~/kent/src/hg/utils/automation/doRecipBest.pl ${rBestTrackHub} -load -workhorse=${workHorse} -buildDir=\`pwd\` \\
   ${tSwapRbestArgs} \\
   ${qSwapRbestArgs} \\
   \${queryDb} \${targetDb}) > rbest.log 2>&1

grep -w real rbest.log | sed -e 's/^/    # /;'

sed -e 's/^/    # /;' fb.\${queryDb}.chainRBest.\${Target}.txt
" > ${swapDir}/runSwap.sh

chmod +x  ${swapDir}/runSwap.sh

printf "# running ${swapDir}/runSwap.sh\n" 1>&2

time (${swapDir}/runSwap.sh) >> ${swapDir}/doSwap.log 2>&1

# rebuild trackDb if possible here
if [ -x "${qTdb}" ]; then
   ${qTdb}
else
   printf "# do not find qTdb '%s'\n" "${qTdb}" 1>&2
fi

fi      ### if [ "$swapDone" -eq 0 ]; then

### continue the make doc

printf "\n    cd ${swapDir}\n" >> ${buildDir}/makeDoc.txt

printf "\n   time (~/kent/src/hg/utils/automation/doBlastzChainNet.pl ${trackHub} -swap -verbose=2 \\
  ${tFullName} ${qFullName} ${buildDir}/DEF -swapDir=\`pwd\` \\
  -syntenicNet -workhorse=${workHorse} -smallClusterHub=${smallClusterHub} -fileServer=${fileServer} -bigClusterHub=${bigHub} \\
    -chainMinScore=${minScore} -chainLinearGap=${linearGap}) > swap.log 2>&1

    grep -w real swap.log | sed -e 's/^/    # /;'
" >> ${buildDir}/makeDoc.txt

(grep -w real ${swapDir}/swap.log || true) | sed -e 's/^/    # /;' >> ${buildDir}/makeDoc.txt

printf "\n    sed -e 's/^/    # /;' fb.${qAsmId}.chain${Target}Link.txt\n" >> ${buildDir}/makeDoc.txt
sed -e 's/^/    # /;' ${swapDir}/fb.${qAsmId}.chain${Target}Link.txt >> ${buildDir}/makeDoc.txt

printf "    sed -e 's/^/    # /;' fb.${qAsmId}.chainSyn${Target}Link.txt\n" >> ${buildDir}/makeDoc.txt
sed -e 's/^/    # /;' ${swapDir}/fb.${qAsmId}.chainSyn${Target}Link.txt >> ${buildDir}/makeDoc.txt

printf "\    time (~/kent/src/hg/utils/automation/doRecipBest.pl ${rBestTrackHub} -load -workhorse=${workHorse} -buildDir=\`pwd\` \\
   ${tSwapRbestArgs} \\
   ${qSwapRbestArgs} \\
   ${qAsmId} ${tAsmId}) > rbest.log 2>&1

    grep -w real rbest.log | sed -e 's/^/    # /;'\n" >> ${buildDir}/makeDoc.txt
(grep -w real ${swapDir}/rbest.log || true) | sed -e 's/^/    # /;' >> ${buildDir}/makeDoc.txt
printf "\n    sed -e 's/^/    # /;' fb.${qAsmId}.chainRBest.${Target}.txt\n" >> ${buildDir}/makeDoc.txt
(sed -e 's/^/    # /;' ${swapDir}/fb.${qAsmId}.chainRBest.${Target}.txt || true) >> ${buildDir}/makeDoc.txt

printf "\n##############################################################################\n" >> ${buildDir}/makeDoc.txt

### show completed makeDoc.txt ####
cat ${buildDir}/makeDoc.txt

### end seconds
export endT=`date "+%s"`

export toAddress="$userName"
export fromAddress="$userName"
export subject="pair lastz DONE $target $query"
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

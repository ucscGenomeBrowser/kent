#!/bin/bash

set -beEu -o pipefail

export TOP="/hive/data/outside/otto/ncbiRefSeq"
cd "${TOP}"

export db="$1"
export Db="$2"
export sumFile="$3"
export asmId="$4"
export wrkDir="$5"
export gffFile="$6"
export msgFile="/tmp/refSeqUpdateNotice.msg.txt"

function prevCompleted() {
  printf "To: hclawson@ucsc.edu,otto-group@ucsc.edu\n" > ${msgFile}
  printf "From: hiram@soe.ucsc.edu\n" >> ${msgFile}
  printf "Subject: $db ncbiRefSeq is not needed\n" >> ${msgFile}
  printf "\n" >> ${msgFile}
  printf "$db ncbiRefSeq is not needed\n" >> ${msgFile}
  cat $msgFile | /usr/sbin/sendmail -t -oi
}

function inProgress() {
  printf "To: hclawson@ucsc.edu,otto-group@ucsc.edu\n" > ${msgFile}
  printf "From: hiram@soe.ucsc.edu\n" >> ${msgFile}
  printf "Subject: ALERT: $db ncbiRefSeq update in progress\n" >> ${msgFile}
  printf "\n" >> ${msgFile}
  printf "ALERT: $db ncbiRefSeq update in progress\n" >> ${msgFile}
  cat $msgFile | /usr/sbin/sendmail -t -oi
}

srcSum=`md5sum "${gffFile}" | cut -d' ' -f1`
prevSum=`cat ${sumFile}`

if [ "${srcSum}" != "${prevSum}" ]; then
  inProgress $db
  if [ ! -d "${wrkDir}" ]; then
     mkdir "${wrkDir}"
     cd "${wrkDir}"
     printf "#!/bin/bash

# otto script created by:
#    /hive/data/outside/ncbi/genomes/ncbiRefSeq/ottoNcbiRefSeq.sh

set -beEu -o pipefail
cd $wrkDir
~/kent/src/hg/utils/automation/doNcbiRefSeq.pl -buildDir=\`pwd\` \\
      -bigClusterHub=ku -dbHost=hgwdev \\
      -fileServer=hgwdev -smallClusterHub=hgwdev -workhorse=hgwdev \\
      ${asmId} ${db}
" > run.sh
     chmod +x run.sh
     time (./run.sh) > do.log 2>&1
     cd "${TOP}"
     printf "%s\n" "${srcSum}" > "${sumFile}"
     /hive/data/outside/ncbi/genomes/ncbiRefSeq/archiveOne.sh "${wrkDir}"
  fi
# else
#   prevCompleted $db
fi


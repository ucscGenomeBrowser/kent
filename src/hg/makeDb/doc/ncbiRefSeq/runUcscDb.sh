#!/bin/bash

set -beEu -o pipefail

export TOP="/hive/data/outside/ncbi/genomes/ncbiRefSeq"
cd "${TOP}"

export db="$1"
export Db="$2"
export sumFile="$3"
export asmId="$4"
export wrkDir="$5"
export gffFile="$6"

srcSum=`md5sum "${gffFile}" | cut -d' ' -f1`
prevSum=`cat ${sumFile}`

if [ "${srcSum}" != "${prevSum}" ]; then
exit 255
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
  fi
else
  printf "# $Db ncbiRefSeq is up to date\n" 1>&2
fi


#!/bin/bash

set -beEu -o pipefail

export PYTHONNOUSERSITE="True"

export TOP="/hive/data/outside/ncbi/genomes/reports/allCommonNames/refSeq"
cd "${TOP}"

export DS=`date "+%F"`
export YYYY=`date "+%Y"`
export logDir="${TOP}/history/${YYYY}"
if [ ! -d "${logDir}" ]; then
  mkdir "${logDir}"
fi

time grep -v "^#" ../../assembly_summary_refseq.txt | cut -f20  \
   | awk -F'/' '{print $NF}' | sed -e '/^na$/d' | sort -u > t0.rs

time grep -v "^#" ../../assembly_summary_refseq_historical.txt | cut -f20  \
   | awk -F'/' '{print $NF}' | sed -e '/^na$/d' | sort -u > t1.rs

sort -u t0.rs t1.rs > refseq.asmId.txt

rm -fr rs.commonNames
mkdir -p rs.commonNames
rm -fr refSeqSplit
mkdir -p refSeqSplit
split --lines=1000 refseq.asmId.txt refSeqSplit/rs.

export DS=`date "+%F"`
ls refSeqSplit >  rs.list.txt
/parasol/bin/gensub2 rs.list.txt single template refseq.jobList
/parasol/bin/para clearSickNodes
/parasol/bin/para flushResults
/parasol/bin/para resetCounts
/parasol/bin/para freeBatch
/parasol/bin/para -ram=3g make refseq.jobList
/parasol/bin/para time > ${logDir}/run.time.${DS}

printf "#### refSeq run complete ${DS}\n"
cat run.time.${DS}
printf "###################################################################\n"

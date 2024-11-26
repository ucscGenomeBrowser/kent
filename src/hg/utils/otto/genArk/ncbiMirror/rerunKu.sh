#!/bin/bash

set -beEu -o pipefail

export PYTHONNOUSERSITE="True"

export TOP="/hive/data/outside/ncbi/genomes/reports/allCommonNames/genBank"
cd "${TOP}"

export DS=`date "+%F"`
export YYYY=`date "+%Y"`
export logDir="${TOP}/history/${YYYY}"
if [ ! -d "${logDir}" ]; then
  mkdir "${logDir}"
fi

time grep -v "^#" ../../assembly_summary_genbank.txt | cut -f20  \
   | awk -F'/' '{print $NF}' | sed -e '/^na$/d' | sort -u > t0.gb

time grep -v "^#" ../../assembly_summary_genbank_historical.txt | cut -f20 \
   | awk -F'/' '{print $NF}' | sed -e '/^na$/d' | sort -u > t1.gb

sort -u t0.gb t1.gb > genbank.asmId.txt

rm -fr gb.commonNames
mkdir -p gb.commonNames
rm -fr genBankSplit
mkdir  genBankSplit
split --lines=1000 genbank.asmId.txt genBankSplit/gb.

ls genBankSplit >  gb.list.txt
/parasol/bin/gensub2 gb.list.txt single template.gb genbank.jobList

ssh ku \
  /hive/data/outside/ncbi/genomes/reports/allCommonNames/genBank/kuBatch.sh \
     < /dev/null

printf "#### genBank run complete ${DS}\n"
cat "${logDir}/run.time.${DS}"
printf "###################################################################\n"

#!/bin/bash

export TOP="/hive/data/outside/ncbi/genomes/reports/allCommonNames/genBank"
cd "${TOP}"

export DS=`date "+%F"`
export YYYY=`date "+%Y"`
export logDir="${TOP}/history/${YYYY}"
if [ ! -d "${logDir}" ]; then
  mkdir "${logDir}"
fi

para clearSickNodes
para flushResults
para resetCounts
para freeBatch
para -ram=3g make genbank.jobList > do.log 2>&1
para time > "${logDir}/run.time.${DS}"

#!/bin/bash

export DS=`date "+%F"`
export Y=`date "+%Y"`
export M=`date "+%m"`

export PYTHONNOUSERSITE="True"

cd /hive/data/outside/ncbi/genomes/reports/allCommonNames

export logFile="log/${Y}/${M}/${DS}.log"
if [ ! -d "log/${Y}/${M}" ]; then
  mkdir -p "log/${Y}/${M}"
fi

function runBoth() {
./genBank/rerunKu.sh &
./refSeq/rerunHgwdev.sh
wait
}

time (runBoth) > "${logFile}" 2>&1

cd /hive/data/outside/ncbi/genomes/reports/allCommonNames

find ./refSeq/rs.commonNames ./genBank/gb.commonNames -type f \
  | grep -v ".txt" | xargs cat \
    | $HOME/bin/x86_64/gnusort -S100G --parallel=32 > asmId.commonName.all.txt



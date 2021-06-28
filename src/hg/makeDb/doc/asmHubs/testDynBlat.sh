#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: testDynBlat.sh asmId > result.json\n" 1>&2
  exit 255
fi

export blatHost="dynablat-01"
export blatPort="4040"

export asmId=$1
export gcX=${asmId:0:3}
export d0=${asmId:4:3}
export d1=${asmId:7:3}
export d2=${asmId:10:3}
export dataDir="$gcX/$d0/$d1/$d2/$asmId"
export srcDir="/hive/data/genomes/asmHubs/$dataDir"
export faaFile="/cluster/home/hiram/kent/src/hg/makeDb/doc/asmHubs/rtp1.ace2.faa.gz"

time (gfClient -genome=$asmId -genomeDataDir=$dataDir -t=dnax -q=prot \
  $blatHost $blatPort $srcDir $faaFile stdout > $asmId.rtp1.ace2.psl || true) 2> $asmId.dynBlat.log

export errCount=`grep -w error $asmId.dynBlat.log | wc -l`
export realTime=`grep -w real $asmId.dynBlat.log | awk '{print $NF}'`
export resultLines=`pslScore $asmId.rtp1.ace2.psl | wc -l`
if [ "${errCount}" -gt 0 ]; then
  printf "%s\t%d\t%s\tERROR\n" "${asmId}" "${resultLines}" "${realTime}"
else
  printf "%s\t%d\t%s\n" "${asmId}" "${resultLines}" "${realTime}"
fi
rm -f $asmId.rtp1.ace2.psl $asmId.dynBlat.log

exit $?

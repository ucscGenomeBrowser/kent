#!/bin/bash

# set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: ./verifyOnDownload.sh <subset.orderList.tsv>\n" 1>&2
  exit 255
fi

#  printf "usage: ./verifyOnDownload.sh <GCF/012/345/678/GCF_012345678.nn>\n" 1>&2
#	${toolsDir}/mkSendList.pl ${orderList} | while read F; do \
#	  ${toolsDir}/verifyOnDownload.sh $$F < /dev/null; done

export orderList=$1
export successCount=0
export doneCount=0

for dirPath in `~/kent/src/hg/makeDb/doc/asmHubs/mkSendList.pl "${orderList}"`
do
  ((doneCount=doneCount+1))

  export genome=`basename $dirPath`

#   hubCount=`wget -O- https://hgdownload.soe.ucsc.edu/hubs/${dirPath}/hub.txt 2> /dev/null | wc -l`
# if [ "${hubCount}" -lt 200 ]; then
#   printf "%d\t%s\tWARNING\n" "${hubCount}" "${dirPath}"
# fi
# else
#  printf "%d\t%s/hub.txt line count\n" "${hubCount}" "${dirPath}"

  trackCount=`curl -L "https://api.genome.ucsc.edu/list/tracks?genome=$genome;trackLeavesOnly=1;hubUrl=https://hgdownload.soe.ucsc.edu/hubs/${dirPath}/hub.txt" \
      2> /dev/null | python -mjson.tool | egrep ": {$" \
       | tr -d '"' | sed -e 's/^ \+//; s/ {//;' | xargs echo | wc -w`
  if [ "${trackCount}" -gt 16 ]; then
    ((successCount=successCount+1))
  fi
  printf "%03d\t%s\t%d tracks:\t" "${doneCount}" "${genome}" "${trackCount}"
  curl -L "https://api.genome.ucsc.edu/list/hubGenomes?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/${dirPath}/hub.txt" 2> /dev/null \
     | python -mjson.tool | egrep "organism\":|description\":" | sed -e "s/'/_/g;" \
       | tr -d '"'  | xargs echo \
          | sed -e 's/genomes: //; s/description: //; s/organism: //; s/{ //g;'
# fi

done
export failCount=`echo $doneCount $successCount | awk '{printf "%d", $1-$2}'`
printf "# checked %3d hubs, %3d success, %3d fail\n" "${doneCount}" "${successCount}" "${failCount}"

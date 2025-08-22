#!/bin/bash

set -beEu -o pipefail

export toolsDir="$HOME/kent/src/hg/makeDb/doc/asmHubs"
export thisDir=`pwd -P`
export whichHub=`basename "${thisDir}" | sed -e 's/AsmHub//;'`

printf "# working: %s\n" "${whichHub}" 1>&2

export jsonDownload="https://hgdownload.soe.ucsc.edu/hubs/${whichHub}/assemblyList.json"
export thisJson="/hive/data/genomes/asmHubs/${whichHub}/assemblyList.json"

if [ "${whichHub}.orderList.tsv" -nt "${thisJson}" ]; then
  make mkJson
fi

# curl -I "${jsonDownload}"
# ls -og "${thisJson}"

export nowTsv="`pwd`/quick.orderList.tsv"

rm -f "${nowTsv}"

${toolsDir}/compareJsonLists.py "${thisJson}" "${jsonDownload}" > "${nowTsv}"

if [ -s "${nowTsv}" ]; then
  printf "make symLinks orderList=${nowTsv}\n" 1>&2
  time (make symLinks orderList=${nowTsv}) >> dbg 2>&1
  printf "make mkGenomes orderList=${nowTsv}\n" 1>&2
  time (make mkGenomes orderList=${nowTsv}) >> dbg 2>&1
  printf "make symLinks orderList=${nowTsv}\n" 1>&2
  time (make symLinks orderList=${nowTsv}) >> dbg 2>&1
  if [ "${whichHub}.orderList.tsv" -nt "/hive/data/genomes/asmHubs/${whichHub}/index.html" ]; then
    printf "make indexPages\n" 1>&2
    time (make indexPages) >> dbg 2>&1
  fi
  printf "make verifyTestDownload orderList=${nowTsv}\n" 1>&2
  time (make verifyTestDownload orderList=${nowTsv}) >> test.down.log 2>&1
  printf "make sendDownload orderList=${nowTsv}\n" 1>&2
  time (make sendDownload orderList=${nowTsv}) >> send.down.log 2>&1
  printf "make verifyDownload orderList=${nowTsv}\n" 1>&2
  time (make verifyDownload orderList=${nowTsv}) >> verify.down.log 2>&1
else
  printf "# quick.orderList is empty, nothing to build\n" 1>&2
fi

rm -f "${nowTsv}"

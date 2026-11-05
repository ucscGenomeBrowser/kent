#!/bin/bash
#############################################################################
###  this script lives in the source tree at:
###      kent/src/hg/utils/otto/genArk/loopDetect.sh
###  edit that copy instead of the in-use copy at:
###      /hive/data/inside/GenArk/pushRR/loopDetect.sh
#############################################################################
###
### detects recursive directory loops in bbi/bbi/... and ixIxx/ixIxx/...
###  sends email notice when found
###
export TOP="/hive/data/inside/GenArk/pushRR"

cd "${TOP}"

export msgTo="hclawson@ucsc.edu"
export loopList=""
export exampleLoops="/tmp/loopExamples.$$.txt"

c=$(zgrep -E -c "bbi/bbi|ixIxx/ixIxx" dev.todayList.gz)
if [ "${c}" -gt 0 ]; then
  printf "hgwdev directory loop detected:\t%d\n" "${c}" >> "${exampleLoops}"
  zgrep -E "bbi/bbi|ixIxx/ixIxx" dev.todayList.gz | head -3 >> "${exampleLoops}"
  loopList="hgwdev ${c}"
fi
c=$(zgrep -E -c "bbi/bbi|ixIxx/ixIxx" hgwbeta.todayList.gz)
if [ "${c}" -gt 0 ]; then
  printf "hgwbeta directory loop detected:\t%d\n" "${c}" >> "${exampleLoops}"
  zgrep -E "bbi/bbi|ixIxx/ixIxx" hgwbeta.todayList.gz | head -3 >> "${exampleLoops}"
  loopList="${loopList} hgwbeta ${c}"
fi
export c=$(zgrep -E -c "bbi/bbi|ixIxx/ixIxx" hgw1.todayList.gz)
if [ "${c}" -gt 0 ]; then
  printf "hgw1 directory loop detected:\t%d\n" "${c}" >> "${exampleLoops}"
  zgrep -E "bbi/bbi|ixIxx/ixIxx" hgw1.todayList.gz | head -3 >> "${exampleLoops}"
  loopList="${loopList} hgw1 ${c}"
fi

if [ "x${loopList}y" != "xy" ]; then
    msgFile="/tmp/loopDetect.$$.txt"
    printf "To: %s\n" "${msgTo}" > "${msgFile}"
    printf "From: hiram@soe.ucsc.edu\n" >> "${msgFile}"
    printf "Subject: ALERT: genArk directory loop detected\n" >> "${msgFile}"
    printf "\n" >> "${msgFile}"
    printf "# directory loops detected: %s\n" "${loopList}" >> "${msgFile}"
    printf "#############################################\n" >> "${msgFile}"
    cat "${exampleLoops}" >> "${msgFile}"
    cat "${msgFile}" | /usr/sbin/sendmail -t -oi
    rm -fr "${msgFile}"
    rm -fr "${exampleLoops}"
fi

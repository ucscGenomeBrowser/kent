#!/bin/sh

# set -beEu -o pipefail

export TOP="/hive/data/outside/grc/incidentDb"
export ECHO="/bin/echo -e"
export failMail="hiram@soe.ucsc.edu"

if [[ $# == 0 || "$1" != "makeItSo" ]]; then
 echo "ERROR: ${TOP}/runUpdate.sh is being run without the argument: makeItSo" \
	| mail -s "ALERT: GRC Incident update" ${failMail} \
	    > /dev/null 2> /dev/null
    ${ECHO} "usage: runUpdate.sh makeItSo"
    ${ECHO} "this script needs the argument: makeItSo"
    ${ECHO} "to make it run.  It will update the GRC incident database"
    ${ECHO} "tracks in the working directory:"
    ${ECHO} "${TOP}"
    ${ECHO} "activity logs can be found in:"
    ${ECHO} "${TOP}/*/*.log.YYYY-mm-dd"
    exit 255
fi

export DS=`date "+%Y-%m-%d"`
export update="/hive/data/outside/grc/incidentDb/update.sh"
mkdir -p ${TOP}/human
cd ${TOP}/human
${update} human hg19 Hg19 GRCh37 > human.log.${DS} 2>&1
mkdir -p ${TOP}/mouse
cd ${TOP}/mouse
${update} mouse mm9 Mm9 MGSCv37 > mouse.log.${DS} 2>&1
mkdir -p ${TOP}/zebrafish
cd ${TOP}/zebrafish
${update} zebrafish danRer7 DanRer7 Zv9 > zebrafish.log.${DS} 2>&1
cd "${TOP}"
./verifyTransfer.sh Hg19 Mm9 DanRer7
if [ $? -ne 0 ]; then
    ${ECHO} "incidentDb/runUpdate.sh failing verifyTransfer.sh" 1>&2
    exit 255
fi
WC=`tail --quiet --lines=1 ${TOP}/human/human.log.${DS} ${TOP}/mouse/mouse.log.${DS} ${TOP}/zebrafish/zebrafish.log.${DS} | grep SUCCESS | wc -l`
if [ "${WC}" -ne 3 ]; then
    ${ECHO} "incidentDb/runUpdate.sh failing" 1>&2
    ${ECHO} "WC: ${WC}" 1>&2
    exit 255
fi

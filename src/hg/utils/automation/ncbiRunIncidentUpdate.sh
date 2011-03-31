#!/bin/sh

set -beEu -o pipefail

export TOP="/hive/data/outside/ncbi/incidentDb"
export ECHO="/bin/echo -e"
export failMail="someUser@domain.top"

if [[ $# == 0 || "$1" != "makeItSo" ]]; then
 echo "ERROR: ${TOP}/runUpdate.sh is being run without the argument: makeItSo" \
	| mail -s "ALERT: NCBI Incident update" ${failMail} \
	    > /dev/null 2> /dev/null
    ${ECHO} "usage: runUpdate.sh makeItSo"
    ${ECHO} "this script needs the argument: makeItSo"
    ${ECHO} "to make it run.  It will update the NCBI incident database"
    ${ECHO} "tracks in the working directory:"
    ${ECHO} "${TOP}"
    ${ECHO} "activity logs can be found in:"
    ${ECHO} "${TOP}/*/*.log.YYYY-mm-dd"
    exit 255
fi

export DS=`date "+%Y-%m-%d"`
export update="/hive/data/outside/ncbi/incidentDb/update.sh"
cd ${TOP}/human
${update} human hg19 Hg19 GRCh37 > human.log.${DS} 2>&1
cd ${TOP}/mouse
${update} mouse mm9 Mm9 MGSCv37 > mouse.log.${DS} 2>&1
cd ${TOP}/zebrafish
${update} zebrafish danRer7 DanRer7 Zv9 > zebrafish.log.${DS} 2>&1

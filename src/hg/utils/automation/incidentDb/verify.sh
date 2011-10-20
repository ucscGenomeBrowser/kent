#!/bin/sh

# these settings will make this script exit on any error at any time
set -beEu -o pipefail

export scriptName="verify"
export previousStep="load"
export TOP="/hive/data/outside/ncbi/incidentDb/testAuto"
export bbiInfo="/cluster/bin/x86_64/bigBedInfo"
export hgsql="/cluster/bin/x86_64/hgsql"

. ${TOP}/commonHeader.inc

export returnCode=0
export tmpFile="./incidentVerify.$$.txt"

verifyOne() {
    export db=$1
    export Db=$2
    cd "${workDir}"
    rm -f "${tmpFile}"
    file="${inProgress}/${Db}.ncbiIncidentDb.bb"
    ${bbiInfo} "${file}" > "${tmpFile}"
    itemCount0=`grep itemCount "${tmpFile}" | awk '{print $2}'`
    sum0=`cat "${tmpFile}" | md5sum | awk '{print $1}'`
    url=`${hgsql} -N -e "select * from nextNcbiIncidentDb;" $db`
    rm -f "${tmpFile}"
    rm -fr ${TOP}/udcCache
    mkdir -p ${TOP}/udcCache
    ${bbiInfo} -udcDir=${TOP}/udcCache "${url}" > "${tmpFile}"
    itemCount1=`grep itemCount "${tmpFile}" | awk '{print $2}'`
    sum1=`cat "${tmpFile}" | md5sum | awk '{print $1}'`
    if [ ${itemCount0} -lt 1 ]; then
        ${ECHO} "ERROR: $db item count in file '${file}' failing: ${itemCount0} < 1" 1>&2
        returnCode=255
    fi
    if [ ${itemCount0} -ne ${itemCount1} ]; then
        ${ECHO} "ERROR: $db item count failing, file != table: ${itemCount0} != ${itemCount1}" 1>&2
        returnCode=255
    fi
    if [ "${sum0}" != "${sum1}" ]; then
        ${ECHO} "ERROR: $db bedInfo check sum failing: ${sum0} != ${sum1}" 1>&2
        returnCode=255
    fi
}

#############################################################################
############  Get to work here

verifyOne hg19 Hg19
verifyOne mm9 Mm9
verifyOne danRer7 DanRer7

cd "${workDir}"
rm -f "${tmpFile}"
if [ "${returnCode}" -eq 0 ]; then
${ECHO} "# ${scriptName} complete "`date '+%Y-%m-%d %H:%M:%S'` > "${signalDone}"
rm -f "${signalRunning}"
fi
exit ${returnCode}

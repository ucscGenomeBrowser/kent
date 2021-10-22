#!/bin/bash

export TOP="/hive/data/outside/grc/incidentDb"
cd "${TOP}"
export ECHO="/bin/echo -e"
export bbiInfo="/cluster/bin/x86_64/bigBedInfo"

if [[ $# == 0 ]]; then
    ${ECHO} "usage: verifyTransfer.sh <DbName> [MoreDbNames]" 1>&2
    ${ECHO} "e.g.: verifyTransfer.sh Hg19 Mm9 DanRer7" 1>&2
    exit 255
fi

export returnCode=0

export tmpFile="/tmp/incidentVerify.$$.txt"
for Db in $*
do
    a=`echo -n "${Db:0:1}" | tr "[:upper:]" "[:lower:]"`
    db=`echo -n "${a}${Db:1}"`
    rm -f "${tmpFile}"
    file="${Db}.grcIncidentDb.bb"
    ${bbiInfo} "${file}" > "${tmpFile}"
    itemCount0=`grep itemCount "${tmpFile}" | sed -e 's/,//g' | awk '{print $2}'`
    sum0=`cat "${tmpFile}" | md5sum | awk '{print $1}'`
    url=`/cluster/bin/x86_64/hgsql -N -e "select * from grcIncidentDb;" $db`
    rm -f "${tmpFile}"
    rm -fr ${TOP}/udcCache
    mkdir ${TOP}/udcCache
    ${bbiInfo} -udcDir=${TOP}/udcCache "${url}" > "${tmpFile}"
    itemCount1=`grep itemCount "${tmpFile}" | sed -e 's/,//g' | awk '{print $2}'`
    sum1=`cat "${tmpFile}" | md5sum | awk '{print $1}'`
    if [ ${itemCount0} -lt 1 ]; then
	${ECHO} "ERROR: item count for $Db failing: ${itemCount0} < 1" 1>&2
    fi
    if [ ${itemCount0} -ne ${itemCount1} ]; then
	${ECHO} "ERROR: item count for $Db failing: ${itemCount0} != ${itemCount1}" 1>&2
	returnCode=255
    fi
    if [ "${sum0}" != "${sum1}" ]; then
	${ECHO} "ERROR: info check sum for $Db failing: ${sum0} != ${sum1}" 1>&2
	returnCode=255
    fi
done

rm -f "${tmpFile}"
exit $returnCode

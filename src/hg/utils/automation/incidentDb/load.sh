#!/bin/sh

# these settings will make this script exit on any error at any time
set -beEu -o pipefail

export scriptName="load"
export previousStep="process"
export TOP="/hive/data/outside/ncbi/incidentDb/testAuto"

. ${TOP}/commonHeader.inc

#############################################################################
loadOne() {
export name=$1
export db=$2
export Db=$3
cd "${workDir}/${name}"

export newSum=`md5sum -b ncbiIncidentDb.bb | awk '{print $1}'`
export oldSum=0
if [ -s "${inProgress}/${Db}.ncbiIncidentDb.bb" ]; then
    oldSum=`md5sum -b "${inProgress}/${Db}.ncbiIncidentDb.bb" | awk '{print $1}'`
else
    touch "${inProgress}/${Db}.ncbiIncidentDb.bb"
fi

if [ "$newSum" != "$oldSum" ]; then
    rm -f "${inProgress}/${Db}.ncbiIncidentDb.bb.prev"
    mv "${inProgress}/${Db}.ncbiIncidentDb.bb" "${inProgress}/${Db}.ncbiIncidentDb.bb.prev"
    ln -s "${workDir}/${name}/ncbiIncidentDb.bb" \
	"${inProgress}/${Db}.ncbiIncidentDb.bb"
fi
}

#############################################################################
############  Get to work here

loadOne human hg19 Hg19
loadOne mouse mm9 Mm9
loadOne zebrafish danRer7 DanRer7

cd "${workDir}"
${ECHO} "# ${scriptName} complete "`date '+%Y-%m-%d %H:%M:%S'` > "${signalDone}"
rm -f "${signalRunning}"
exit 0

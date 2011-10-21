#!/bin/sh

# these settings will make this script exit on any error at any time
set -beEu -o pipefail

export scriptName="push"
export previousStep="verify"
export TOP="/hive/data/outside/ncbi/incidentDb/testAuto"

. ${TOP}/commonHeader.inc

#############################################################################
pushOne() {
export name=$1
export Db=$2
cd "${workDir}/${name}"

export newSum=0
if [ -s "${inProgress}/${Db}.ncbiIncidentDb.bb" ]; then
    newSum=`md5sum -b "${inProgress}/${Db}.ncbiIncidentDb.bb" | awk '{print $1}'`
fi
export existingSum=0
if [ -s "${pushed}/${Db}.ncbiIncidentDb.bb" ]; then
    existingSum=`md5sum -b "${pushed}/${Db}.ncbiIncidentDb.bb" | awk '{print $1}'`
fi

if [ "$newSum" != "$existingSum" ]; then
    rm -f "${pushed}/${Db}.ncbiIncidentDb.bb"
    ln -s "${workDir}/${name}/ncbiIncidentDb.bb" \
	"${pushed}/${Db}.ncbiIncidentDb.bb"
else
    cd "${workDir}"
    ${ECHO} "# "`date '+%Y-%m-%d %H:%M:%S'` > "please.clean.${name}"
fi
}

#############################################################################
############  Get to work here

pushOne human Hg19
pushOne mouse Mm9
pushOne zebrafish DanRer7

cd "${workDir}"
${ECHO} "# ${scriptName} complete "`date '+%Y-%m-%d %H:%M:%S'` > "${signalDone}"
rm -f "${signalRunning}"
exit 0

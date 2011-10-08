#!/bin/sh

# these settings will make this script exit on any error at any time
set -beEu -o pipefail

export scriptName="cleanup"
export previousStep="push"
export TOP="/hive/data/outside/ncbi/incidentDb/testAuto"

. ${TOP}/commonHeader.inc

#############################################################################
checkOne() {
name=$1
cd "${workDir}"

if [ -s "please.clean.${name}" ]; then
    echo "cleaning $name"
    rm -fr ./${name}/
fi
}

checkOne human
checkOne mouse
checkOne zebrafish

cd "${workDir}"
${ECHO} "# ${scriptName} complete "`date '+%Y-%m-%d %H:%M:%S'` > "${signalDone}"
rm -f "${signalRunning}"
exit 0

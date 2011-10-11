# common header to include in all scripts to verify startup business

export ECHO="/bin/echo -e"
if [ $# -ne 1 ]; then
    ${ECHO} "ERROR: missing workDir argument to ${scriptName}.sh" 1>&2
    exit 255
fi

# this script will keep its download business in TOP/<species>
# same download directory all the time
export workDir=$1
export signalDone="${scriptName}.done"
export signalRunning="${scriptName}.running"
export previousDone="${previousStep}.done"
export inProgress="${TOP}/inProgress"
export pushed="${TOP}/pushed"

if [ ! -d "${workDir}" ]; then
    ${ECHO} "ERROR: given workDir '${workDir}' is not a directory" 1>&2
    exit 255
fi

cd "${workDir}"
# exit silently if already done
if [ -s "${signalDone}" ]; then
    exit 0
fi

${ECHO} "# starting ${scriptName} "`date '+%Y-%m-%d %H:%M:%S'`
${ECHO} "# in workDir "`pwd`

if [ "x${previousStep}y" != "xy" -a ! -s "${previousDone}" ]; then
    ${ECHO} "ERROR: ${scriptName}.sh called before ${previousStep} done" 1>&2
    exit 255
fi

if [ -s "${signalRunning}" ]; then
    ${ECHO} "ERROR: ${scriptName}.sh called while a ${scriptName}.sh is running."
    exit 255
fi
${ECHO} "# ${scriptName} running "`date '+%Y-%m-%d %H:%M:%S'` > ${signalRunning}

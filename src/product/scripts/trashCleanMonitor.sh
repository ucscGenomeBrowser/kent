#!/bin/sh
#
#	trashCleanMonitor.sh - script to call trashCleaner.csh and verify
#		that it actually worked OK
#
export trashDir="/data/apache/trash"
export trashCt="${trashDir}/ct"
export userData="/data/apache/userdata"
export userCt="${userData}/ct"
export userLog="${userData}/log"
export dateStamp=`date "+%Y-%m-%dT%H"`
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export logDir="${userLog}/${YYYY}/${MM}"
export cleanerLog="${logDir}/cleanerLog.${dateStamp}.txt"
export trashCleaner="$HOME/kent/src/product/scripts/trashCleaner.csh"
export failMail="alert@some.where"
export failMessage="ALERT: from trashCleanMonitor.sh - the trash cleaner is failing, check ${cleanerLog}"

export ECHO="/bin/echo -e"

if [ "$1" != "searchAndDestroy" ]; then
    ${ECHO} "usage:  trashCleanMonitor.csh searchAndDestroy"
    ${ECHO} "This script will run when given the argument searchAndDestroy"
    ${ECHO} "which is exactly what it will do to the trash files for the"
    ${ECHO} "genome browser.  There is no turning back after it gets going."
    ${ECHO} "This script is actually a monitor on the actual trash cleaner:"
    ${ECHO} "\t${trashCleaner}"
    ${ECHO} "It will verify the trash cleaner is functioning properly and if"
    ${ECHO} "there are problems it will email a failure message to"
    ${ECHO} "\t${failMail}"
    ${ECHO} "Files that belong to sessions will be moved from the directory"
    ${ECHO} "${trashCt} to ${userCt}"
    ${ECHO} "activity logs can be found in ${userLog}"
    exit 255
fi

if [ ! -d "${logDir}" ]; then
    mkdir -p "${logDir}"
    chmod 755 "${userLog}"
    chmod 755 "${userLog}/${YYYY}"
    chmod 755 "${logDir}"
fi
$trashCleaner searchAndDestroy > "${cleanerLog}" 2>&1
returnCode=$?
if [ "${returnCode}" -eq "0" ]; then
    lastLine=`tail --lines=1 "${cleanerLog}" | sed -e "s/ trash clean.*//"`
    if [ "${lastLine}" != "SUCCESS" ]; then
	${ECHO} "${failMessage}" \
	    | mail -s "ALERT: TRASH" ${failMail} > /dev/null 2> /dev/null
	exit 255
    fi
else
    ${ECHO} "${failMessage}" \
	| mail -s "ALERT: TRASH" ${failMail} > /dev/null 2> /dev/null
    exit 255
fi

exit 0

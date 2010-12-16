#!/bin/sh
#
#	trashCleanMonitor.sh - script to call trashCleaner.csh and verify
#		that it actually worked OK
#
# make sure we are in this work directory:
cd /home/qateam/trashCleaners/rr

# this lockFile must NOT exist for this to function.  This script will
#	create it and the trash clean script will expect it to exist
#	and will remove it.
export lockFile="/export/userdata/cleaner.pid"
export trashDir="/export/trash"
export trashCt="${trashDir}/ct"
export userData="/export/userdata"
export userCt="${userData}/ct/rr"
export userLog="${userData}/rrLog"
export dateStamp=`date "+%Y-%m-%dT%H"`
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export logDir="${userLog}/${YYYY}/${MM}"
export cleanerLog="${logDir}/cleanerLog.${dateStamp}.txt"
export trashCleaner="/home/qateam/trashCleaners/rr/trashCleaner.csh"
export failMail="alertAddress@yourDomain.com"
export failMessage="ALERT: from hgnfs1/rr trashCleanMonitor.sh - the trash cleaner is failing, check hgnfs1 ${cleanerLog}"

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

if [ -f "${lockFile}" ]; then
    ${ECHO} "lockFile ${lockFile} exists" \
	| mail -s "ALERT: hgnfs1 RR" ${failMail} > /dev/null 2> /dev/null
    exit 255
else
    ${ECHO} "rr monitor $$ "`date +%Y-%m-%dT%H:%M:%S` > "${lockFile}"
    chmod 666 "${lockFile}"
fi

if [ ! -d "${logDir}" ]; then
    mkdir -p "${logDir}"
    chmod 755 "${userLog}"
    chmod 755 "${userLog}/${YYYY}"
    chmod 755 "${logDir}"
fi
touch "${cleanerLog}"
chmod 666 "${cleanerLog}"
$trashCleaner searchAndDestroy > "${cleanerLog}" 2>&1
returnCode=$?
if [ "${returnCode}" -eq "0" ]; then
    lastLine=`tail --lines=1 "${cleanerLog}" | sed -e "s/ trash clean.*//"`
    if [ "${lastLine}" != "SUCCESS" ]; then
	( ${ECHO} "${failMessage}"; tail "${cleanerLog}" ) \
	    | mail -s "ALERT: hgnfs1 RR TRASH (rc0)" ${failMail} > /dev/null 2> /dev/null
	exit 255
    fi
else
    ( ${ECHO} "${failMessage}"; tail "${cleanerLog}" ) \
	| mail -s "ALERT: hgnfs1 RR TRASH" ${failMail} > /dev/null 2> /dev/null
    exit 255
fi

exit 0

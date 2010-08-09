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
export considerRemoval="${userLog}/considerRemoval.txt"
export dateStamp=`date "+%Y-%m-%dT%H"`
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export logDir="${userLog}/${YYYY}/${MM}"
export cleanerLog="${logDir}/cleanerLog.${dateStamp}.txt"
export dbTrashLog="${logDir}/dbTrash.${dateStamp}.txt"
export trashLog="${logDir}/trash.${YYYY}-${MM}.txt"
export trashCleaner="$HOME/kent/src/product/scripts/trashCleaner.csh"

export ECHO="/bin/echo -e"

${ECHO} "Content-type: text/html"
${ECHO}
${ECHO} "<HTML><HEAD><TITLE>trash clean and monitor</TITLE></HEAD>"
${ECHO} "<BODY>"
${ECHO} "<HR>"
${ECHO} "<H4>running trash cleaner:</H4>"
${ECHO} "<HR>"
${ECHO} "<PRE>"
$trashCleaner > "${cleanerLog}" 2>&1
returnCode=$?
if [ "${returnCode}" -eq "0" ]; then
    lastLine=`tail --lines=1 "${cleanerLog}" | sed -e "s/ trash clean.*//"`
    if [ "${lastLine}" != "SUCCESS" ]; then
	${ECHO} "ERROR: trashCleaner has zero return code, but no success message"
	${ECHO} "ERROR: Some of the log:"
	tail --lines=10 "${cleanerLog}"
    else
	tail --lines=1 "${cleanerLog}"
    fi
else
    ${ECHO} "ERROR: trashCleaner has failure exit code.  Some of the log:"
    tail --lines=10 "${cleanerLog}"
fi

${ECHO} "</PRE>"
${ECHO} "<HR>"
${ECHO} "</BODY></HTML>"
exit 0

#!/bin/bash
#
#	trashCleanMonitor.sh - script to call trashCleaner.bash and verify
#		that it actually worked OK
#
# different echo commands for different systems, the -e means:
#    -e     enable interpretation of backslash escapes
# a typical feature found in the GNU echo command
export ECHO="/bin/echo -e"

# address list, comma separated addresses, to send notifications
export failMail="alert@some.where,extraAlert@other.where"

# a simple command line mail program to use the failMail address list
# to email messages from this cron job.  (TODO: this should instead be sendmail
# with appropriate changes below to use sendmail properly)
export mailCmd="/bin/mail"
# when there is no mail system, set this to echo:
# export mailCmd="/bin/echo"

usage() {
    ${ECHO} "usage:  trashCleanMonitor.csh <browserEnvironment.txt> searchAndDestroy" 1>&2
    ${ECHO} "  the <browserEnvironment.txt> file contains definitions of how" 1>&2
    ${ECHO} "  these scripts behave in your local environment." 1>&2
    ${ECHO} "This script will run when given the argument searchAndDestroy" 1>&2
    ${ECHO} "which is exactly what it will do to the trash files for the" 1>&2
    ${ECHO} "genome browser.  There is no turning back after it gets going." 1>&2
    ${ECHO} "This script is actually a monitor on the actual trash cleaner:" 1>&2
    ${ECHO} "\tKENTHOME/src/product/scripts/trashCleaner.bash" 1>&2
    ${ECHO} "It will verify the trash cleaner is functioning properly and if" 1>&2
    ${ECHO} "there are problems it will email a failure message to" 1>&2
    ${ECHO} "\t${failMail}" 1>&2
    ${ECHO} "Files that belong to sessions will be moved from the directory" 1>&2
    ${ECHO} "TRASHDIR/ct/ to USERDATA/ct/" 1>&2
    ${ECHO} "activity logs can be found in USERDATA/log/YYYY/MM" 1>&2
    exit 255
}

export includeFile=$1
if [ "X${includeFile}Y" = "XY" ]; then
    usage
fi

if [ -f "${includeFile}" ]; then
    . "${includeFile}"
else
    ${ECHO} "trashCleanMonitor.sh: ERROR: can not find ${includeFile}" 1>&2
    ${ECHO} "trashCleanMonitor.sh: ERROR: can not find ${includeFile}" \
      | ${mailCmd} -s "ALERT:" ${failMail} > /dev/null 2> /dev/null
    usage
fi

if [ -z "${TRASHDIR+x}" ]; then
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for TRASHDIR" 1>&2
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for TRASHDIR" \
      | ${mailCmd} -s "ALERT:" ${failMail} > /dev/null 2> /dev/null
  exit 255
fi
if [ -z "${USERDATA+x}" ]; then
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for USERDATA" 1>&2
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for USERDATA" \
      | ${mailCmd} -s "ALERT:" ${failMail} > /dev/null 2> /dev/null
  exit 255
fi
if [ -z "${KENTHOME+x}" ]; then
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for KENTHOME" 1>&2
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for KENTHOME" \
      | ${mailCmd} -s "ALERT:" ${failMail} > /dev/null 2> /dev/null
  exit 255
fi
export trashDir="${TRASHDIR}"
export trashCt="${trashDir}/ct"
export userData="${USERDATA}"
export lockFile="${userData}/cleaner.pid"
export userCt="${userData}/ct"
export userLog="${userData}/log"
export dateStamp=`date "+%Y-%m-%dT%H"`
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export logDir="${userLog}/${YYYY}/${MM}"
export cleanerLog="${logDir}/cleanerLog.${dateStamp}.txt"
export trashCleaner="$KENTHOME/src/product/scripts/trashCleaner.bash"
export scriptsDir="$SCRIPTS"
export failMessage="ALERT: from trashCleanMonitor.sh - the trash cleaner is failing, check the most recent file(s) in /var/tmp/ for clues, or perhaps in the ${cleanerLog}"


if [ "$2" != "searchAndDestroy" ]; then
    ${ECHO} "trashCleanMonitor.sh: ERROR: being run without arguments ?"
    ${ECHO} "trashCleanMonitor.sh: ERROR: being run without arguments ?" \
      | ${mailCmd} -s "ALERT:" ${failMail} > /dev/null 2> /dev/null
    usage
fi

if [ -f "${lockFile}" ]; then
    ${ECHO} "lockFile ${lockFile} exists" \
        | ${mailCmd} -s "ALERT:" ${failMail} > /dev/null 2> /dev/null
    ps -ef | grep "trashCleaner.bash" | grep -v "grep" \
        | ${mailCmd} -s "ALERT:" ${failMail} > /dev/null 2> /dev/null
    exit 255
else
    ${ECHO} "monitor $$ "`date +%Y-%m-%dT%H:%M:%S` > "${lockFile}"
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
    lastLine=`tail --lines=4 "${cleanerLog}" | grep "^SUCCESS" | sed -e "s/ trash clean.*//"`
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

# it is expected that trashCleaner.bash will remove the lock file
# when it can exit successfully

# rm -f "${lockFile}"

exit 0

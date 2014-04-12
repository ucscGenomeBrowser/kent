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

# a simple command line mail program to use the failMail address
# list to email messages from this cron job
# export mailCmd="/bin/mail -s"
# there is no mail command in this system
export mailCmd="/bin/echo"

usage() {
    ${ECHO} "usage:  trashCleanMonitor.sh <browserEnvironment.txt> searchAndDestroy" 1>&2
    ${ECHO} "  the <browserEnvironment.txt> file contains definitions of how" 1>&2
    ${ECHO} "  these scripts behave in your local environment." 1>&2
    ${ECHO} "This script will run when given the argument searchAndDestroy" 1>&2
    ${ECHO} "which is exactly what it will do to the trash files for the" 1>&2
    ${ECHO} "genome browser.  There is no turning back after it gets going." 1>&2
    ${ECHO} "This script is actually a monitor on the actual trash cleaner:" 1>&2
    ${ECHO} "\tKENTHOME/kent/src/product/scripts/trashCleaner.bash" 1>&2
    ${ECHO} "It will verify the trash cleaner is functioning properly and if" 1>&2
    ${ECHO} "there are problems it will email a failure message to" 1>&2
    ${ECHO} "\t${failMail}" 1>&2
    ${ECHO} "Files that belong to sessions will be moved from the directory" 1>&2
    ${ECHO} "TRASHDIR/ct/ to USERDATA/ct/" 1>&2
    ${ECHO} "activity logs can be found in LOGDIR/trashCleaner/YYYY/MM" 1>&2
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
      | ${mailCmd} "ALERT:" ${failMail} > /dev/null 2> /dev/null
    usage
fi

if [ -z "${TMPDIR+x}" ]; then
  if [ -d "/var/tmp" ]; then
    export TMPDIR="/var/tmp"
  else
    export TMPDIR="/tmp"
  fi
fi

if [ -z "${TRASHDIR+x}" ]; then
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for TRASHDIR" 1>&2
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for TRASHDIR" \
      | ${mailCmd} "ALERT:" ${failMail} > /dev/null 2> /dev/null
  exit 255
fi
if [ -z "${USERDATA+x}" ]; then
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for USERDATA" 1>&2
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for USERDATA" \
      | ${mailCmd} "ALERT:" ${failMail} > /dev/null 2> /dev/null
  exit 255
fi
if [ -z "${KENTHOME+x}" ]; then
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for KENTHOME" 1>&2
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for KENTHOME" \
      | ${mailCmd} "ALERT:" ${failMail} > /dev/null 2> /dev/null
  exit 255
fi
if [ -z "${LOGDIR+x}" ]; then
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for LOGDIR" 1>&2
  ${ECHO} "trashCleanMonitor.sh: ERROR: missing definition for LOGDIR" \
      | ${mailCmd} "ALERT:" ${failMail} > /dev/null 2> /dev/null
  exit 255
fi
export trashDir="${TRASHDIR}"
export trashCt="${trashDir}/ct"
export userData="${USERDATA}"
export lockFile="${userData}/cleaner.pid"
export userCt="${userData}/ct"
export userLog="${LOGDIR}/trashCleaner"
export dateStamp=`date "+%Y-%m-%dT%H"`
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export logDir="${LOGDIR}/trashCleaner/${YYYY}/${MM}"
export cleanerLog="${logDir}/cleanerLog.${dateStamp}.txt"
export scriptsDir="/root/browserLogs/scripts"
export trashCleaner="${scriptsDir}/trashCleaner.bash"
export failMessage="ALERT: from trashCleanMonitor.sh - the trash cleaner is failing, check the most recent file(s) in /var/tmp/ for clues, or perhaps in the ${cleanerLog}"

if [ "$2" != "searchAndDestroy" ]; then
    ${ECHO} "trashCleanMonitor.sh: ERROR: being run without arguments ?"
    ${ECHO} "trashCleanMonitor.sh: ERROR: being run without arguments ?" \
      | ${mailCmd} "ALERT:" ${failMail} > /dev/null 2> /dev/null
    usage
fi

if [ -f "${lockFile}" ]; then
    ${ECHO} "lockFile ${lockFile} exists" 1>&2
    ${ECHO} "lockFile ${lockFile} exists" \
        | ${mailCmd} "ALERT:" ${failMail} > /dev/null 2> /dev/null
    ps -ef | grep "trashCleaner.bash" | grep -v "grep" \
        | ${mailCmd} "ALERT:" ${failMail} > /dev/null 2> /dev/null
    exit 255
else
    ${ECHO} "monitor $$ "`date +%Y-%m-%dT%H:%M:%S` > "${lockFile}"
    chmod 666 "${lockFile}"
fi


if [ ! -d "${logDir}" ]; then
    mkdir -p "${logDir}"
    chmod 755 "${LOGDIR}/trashCleaner"
    chmod 755 "${LOGDIR}/trashCleaner/${YYYY}"
    chmod 755 "${logDir}"
fi
touch "${cleanerLog}"
chmod 666 "${cleanerLog}"

$trashCleaner searchAndDestroy >> "${cleanerLog}" 2>&1
returnCode=$?
if [ "${returnCode}" -eq "0" ]; then
    lastLine=`tail --lines=1 "${cleanerLog}" | sed -e "s/ trash clean.*//"`
    if [ "${lastLine}" != "SUCCESS" ]; then
	${ECHO} "${failMessage}" 1>&2
	${ECHO} "${failMessage}" \
	    | ${mailCmd} "ALERT: TRASH" ${failMail} > /dev/null 2> /dev/null
	exit 255
    fi
else
    ${ECHO} "${failMessage}" 1>&2
    ${ECHO} "${failMessage}" \
	| ${mailCmd} "ALERT: TRASH" ${failMail} > /dev/null 2> /dev/null
    exit 255
fi

# it is expected that trashCleaner.bash will remove the lock file
# when it can exit successfully

# rm -f "${lockFile}"

exit 0

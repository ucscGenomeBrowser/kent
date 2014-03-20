#!/bin/bash

# trashSizeMonitor.sh - quickly measure size and age of files in /trash/
#
#   returns: path name to a two column file with last access time and file name
#            for each file in the /trash/ directory.
#
# requires external scripts: fileStatsFromFind.pl and dirStatsFromFind.pl
#
# this script is used by the trashCleaner.csh script and the
#   measureTrash.sh script
#

# exit immediately on any command failure
set -beEu -o pipefail

# different echo commands for different systems, the -e means:
#    -e     enable interpretation of backslash escapes
# a typical feature found in the GNU echo command
export ECHO="/bin/echo -e"
#
if [ -z "${TMPDIR+x}" ]; then
  if [ -d "/var/tmp" ]; then
    export TMPDIR="/var/tmp"
  else
    export TMPDIR="/tmp"
  fi
fi
if [ -z "${KENTHOME+x}" ]; then
  ${ECHO} "ERROR: trashSizeMonitor.sh needs to have KENTHOME defined" 1>&2
  exit 255
fi
if [ -z "${LOGDIR+x}" ]; then
  ${ECHO} "ERROR: trashSizeMonitor.sh needs to have LOGDIR defined" 1>&2
  exit 255
fi
export lockFile="$TMPDIR/trashSizeMonitor.pid"
# trashDir may be defined by the caller, needs to be an explicit
# full path to a real directory, not a symlink to trash/
if [ -z "${trashDir+x}" ]; then
  export TRASH="/export/trash"
else
  export TRASH="${trashDir}"
fi
${ECHO} "working on TRASH: $TRASH" 1>&2
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export DS=`date "+%Y-%m-%d.%H:%M:%S"`
export DST=`date "+%Y-%m-%dT%H:%M:%S"`
# to check if this is a 'root' cron job
export ID=`id -u -n`

# note, this name is expected to have a pattern used below
export trashFileList=`mktemp $TMPDIR/trashFileList.XXXXXX`
chmod 666 "${trashFileList}"

runMonitor()
{
# the following measurement scripts need something to work, so in case
# all of the trash hierarchy is empty, give it something to work on:
${ECHO} "some trash" > "${TRASH}/hgt/someTrash.png"
# this find can take up to 20 minutes for a filesystem with 2 million files
# it depends upon the type of filesystem
nice -n 19 find ${TRASH} -ignore_readdir_race -xdev -type f 2> /dev/null > "${trashFileList}"
$KENTHOME/kent/src/product/scripts/fileStatsFromFind.pl "${trashFileList}"
export dirList=`sed -e 's#.*/trash/##;' "${trashFileList}" | grep "/" | sed -e 's#/.*##' | sort -u | xargs ${ECHO}`
$KENTHOME/kent/src/product/scripts/dirStatsFromFind.pl "${trashFileList}" $dirList
${ECHO} "# trash monitor done: "`date +%Y-%m-%dT%H:%M:%S`
}

if [ -f "${lockFile}" ]; then
  ${ECHO} "ERROR: trashSizeMonitor.sh monitor already running" 1>&2
  ${ECHO} "lockFile: $lockFile" 1>&2
  exit 255
else
  ${ECHO} "monitor $$ "`date +%Y-%m-%dT%H:%M:%S` > "${lockFile}"
  chmod 666 "${lockFile}"
fi

export LOG="${LOGDIR}/trashLog/${YYYY}/${MM}/${DS}"
export MONTHLOG=`dirname ${LOG}`
if [ ! -d "${MONTHLOG}" ]; then
  mkdir -p "${MONTHLOG}"
# this may be a 'root' cron job where it is more convenient
# to have log files owned by an ordinary user
#    if [ "${ID}" = "root" ]; then
#        chown someUser:someGroup ${LOGDIR}/trashLog/${YYYY}/${MM}
#        chown someUser:someGroup ${LOGDIR}/trashLog/${YYYY}
#        chown someUser:someGroup ${LOGDIR}/trashLog
#        chown someUser:someGroup ${LOGDIR}
#    fi
fi

${ECHO} "# trashSizeMonitor.sh writing to LOG: ${LOG}" 1>&2

runMonitor > ${LOG} 2>&1
chmod 666 ${LOG}
# if [ "${ID}" = "root" ]; then
#    chown someUser:someGroup ${LOG}
# fi
df -k > "${MONTHLOG}/df-k.${DST}.txt"
chmod 666 "${MONTHLOG}/df-k.${DST}.txt"
# if [ "${ID}" = "root" ]; then
#    chown someUser:someGroup ${MONTHLOG}/df-k.${DST}.txt
# fi
# cd /home/qateam/dataAnalysis/trashFileCounts
# ./fileCounts.pl ../../trashLog/${YYYY} 2> /dev/null | egrep "^#|^chr1" | sort -k2,2n > ${YYYY}.file.counts.txt 
# chmod 666 ${YYYY}.file.counts.txt

# the dirStatsFromFind.pl script constructed this atimeFile
# temporary file: /var/tmp/trash.atime.xxxxxx
# a two column file, column 1 is the last access time in seconds since epoch
# column 2 is the full pathname of a file in trash
export atimeFile=`${ECHO} "${trashFileList}" | sed -e 's/trashFileList./trash.atime./'`
chmod 666 "${atimeFile}"
# return the atime file list to the caller to be used there
${ECHO} "${atimeFile}"
rm -f "${lockFile}" "${trashFileList}"
sleep 1

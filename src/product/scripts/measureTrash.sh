#!/bin/bash

# measureTrash.sh - write to log file measurements of /trash/ directory
#                   contents
#   requires external script: HOME/kent/src/product/scripts/trashSizeMonitor.sh
#
# can be used as a cron job to record state of trash directory usage

# exit immediately on any command failure
set -beEu -o pipefail

# different echo commands for different systems, the -e means:
#    -e     enable interpretation of backslash escapes
# a typical feature found in the GNU echo command
export ECHO="/bin/echo -e"

usage() {
  ${ECHO} "usage: measureTrash.sh <browserEnvironment.txt>" 1>&2
  ${ECHO} "  the <browserEnvironment.txt> file contains definitions of how" 1>&2
  ${ECHO} "  these scripts behave in your local environment." 1>&2
  ${ECHO} "This script will measure the /trash/ directory contents." 1>&2
  ${ECHO} "producing a log file of results in LOGDIR/trashLog/YYYY/MM/" 1>&2
  exit 255
}

if [ -z "${1+x}" ]; then
    usage
fi

export includeFile=$1
if [ "X${includeFile}Y" = "XY" ]; then
    usage
fi

if [ -f "${includeFile}" ]; then
    . "${includeFile}"
else
    ${ECHO} "measureTrash.sh: ERROR: can not find ${includeFile}" 1>&2
    usage
fi

if [ -z "${USERDATA+x}" ]; then
  ${ECHO} "measureTrash.sh: ERROR: missing definition for USERDATA" 1>&2
  exit 255
fi
if [ -z "${TRASHDIR+x}" ]; then
  ${ECHO} "measureTrash.sh: ERROR: missing definition for TRASHDIR" 1>&2
  exit 255
fi

export trashDir="${TRASHDIR}"
export userData="${USERDATA}"
export lockFile="${userData}/cleaner.pid"

# do not want to overlap with the trash cleaner
# do not run if the trash cleaner is already running
if [ -f "${lockFile}" ]; then
  ${ECHO} "measureTrash.sh: ERROR: the trash cleaner is running at this time" 1>&2
  exit 0
fi

# this measurement script doesn't care about the return results
# from the trashSizeMonitor.sh
export atimeFile=`${KENTHOME}/kent/src/product/scripts/trashSizeMonitor.sh`

# so remove it if it is created
if [ -e "${atimeFile}" ]; then
   rm -f "${atimeFile}"
fi


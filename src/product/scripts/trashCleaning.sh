#!/bin/sh
#
#	$Id: trashCleaning.sh,v 1.1.2.1 2010/03/22 19:10:07 hiram Exp $
#
usage() {
    echo "usage: trashCleaning.sh <browserEnvironment.txt>"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "This script should be run periodically to clean out unused files"
    echo "  in your specified TRASHDIR directory.  Logs of cleaned data"
    echo "  statistics can be found in your specified LOGDIR/trashLog/YYYY"
    echo "This script must be run as the root user since it needs to clean"
    echo "  files owned by Apache.  It can be a dangerous script, you will"
    echo "  want to convince yourself that it really is cleaning the"
    echo "  correct directory TRASHDIR as specified in browserEnvironment.txt"
    echo "Two different time periods of cleaning are specified, here."
    echo "  One of 8 hours since last access for most files in trash,"
    echo "  and one of 72 hours for custom track files in trash."
    exit 255
}

##########################################################################
# Minimal argument checking and use of the specified include file

if [ $# -ne 1 ]; then
    usage
fi

export includeFile=$1
if [ "X${includeFile}Y" = "XY" ]; then
    usage
fi

if [ -f "${includeFile}" ]; then
    . "${includeFile}"
else
    echo "ERROR: trashCleaning.sh: can not find ${includeFile}"
    usage
fi

##########################################################################
#  verify TRASHDIR is a valid pathname, must not be empty
if [ -z "${TRASHDIR}" ]; then
    echo "ERROR: dbTrashCleaner.sh - TRASHDIR variable is not set in"
    echo "       your browser environment specification file."
    exit 255
fi

#	must be absolute
if [[ ! "${TRASHDIR}" =~ "^/" ]]; then
    echo "ERROR: dbTrashCleaner.sh - TRASHDIR variable is not an absolute path"
    echo "       '${TRASHDIR}'"
    exit 255
fi

#	must end in /trash
checkName=`basename "${TRASHDIR}"`
if [ "X${checkName}Y" != "XtrashY" ]; then
    echo "ERROR: dbTrashCleaner.sh - TRASHDIR variable does not end in /trash"
    echo "       '${TRASHDIR}'"
    exit 255
fi

#	And it needs to exist
if [ ! -d "${TRASHDIR}" ]; then
    echo "ERROR: dbTrashCleaner.sh - TRASHDIR does not exist"
    echo "       '${TRASHDIR}'"
    exit 255
fi

##########################################################################
#	Establish date/time stamps for the log file name
DS=`date "+%Y-%m-%d"`
YYYY=`date "+%Y"`
MM=`date "+%m"`
HOUR=`date "+%H"`
export DS YYYY MM HOUR

# Keep a log of TRASH cleaning
cleanLogDir="${LOGDIR}/trashLog/${YYYY}"
LOGFILE="${cleanLogDir}/${YYYY}-${MM}.txt"
if [ ! -d "${cleanLogDir}" ]; then
  mkdir -p "${cleanLogDir}"
fi
#	first time in this logfile, add comments at the top of the file
if [ ! -s "${LOGFILE}" ]; then
    echo -e "# date\t\t8-hour\t\t72-hour" >> "${LOGFILE}"
    echo -e "#\t\tKbytes\t\tKbytes" >> "${LOGFILE}"
fi

kbBefore=`du --apparent-size -ksc ${TRASHDIR} | grep "${TRASHDIR}" | awk '{print $1}'`

#  This is the 8-hour remove command, note the find works directly on the
#	specified full path TRASHDIR.  This 8-hour since last access cleaner
#	avoids files in trash/ct/ and trash/hgSs/ to leave custom tracks
#	existing.
find ${TRASHDIR} \! \( -regex "${TRASHDIR}/ct/.*" -or -regex "${TRASHDIR}/hgSs/.*" \) \
    -type f -amin +480 -exec rm -f {} \;

kbAfter=`du --apparent-size -ksc ${TRASHDIR} | grep "${TRASHDIR}" | awk '{print $1}'`
eightHourClean=`echo $kbBefore $kbAfter | awk '{printf "%d", $1-$2}'`
kbBefore=$kbAfter

#  This is the 72-hour remove command, note the find works directly on the
#	specified full path TRASHDIR.  This 72-hour since last access cleaner
#	works only on files in trash/ct/ and trash/hgSs/
find ${TRASHDIR} \( -regex "${TRASHDIR}/ct/.*" -or -regex "${TRASHDIR}/hgSs/.*" \) \
    -type f -amin +4320 -exec rm -f {} \;

kbAfter=`du --apparent-size -ksc ${TRASHDIR} | grep "${TRASHDIR}" | awk '{print $1}'`
seventyTwoHourClean=`echo $kbBefore $kbAfter | awk '{printf "%d", $1-$2}'`

if [ ! -s "${LOGFILE}" ]; then
  echo "# trash cleaning log, units in Kb, 8 hour and 72 hour time periods" \
    > ${LOGFILE}
  echo -e "#     date\t8 hour\t72 hour" >> ${LOGFILE}
fi

echo -e "${DS}T${HOUR}\t${eightHourClean}\t${seventyTwoHourClean}" >> ${LOGFILE}
#	This script needs to run as root, fixup the perms so that other
#	users can manipulate the log files
chmod 666 ${LOGFILE}

#!/bin/bash
#
# NOTE: this script is to be used by the trashCleanMonitor.sh script
#       this is NOT for individual use by itself.
#
# exit on any error at any time
set -beEu -o pipefail

# all files should be permissions 'rw-' for 'user' 'group' 'other'
umask 0

# debugging trace all commands, uncomment this set -x statement:
# set -x
#
#	trashCleaner.bash - script to save session user data into a special
#		area outside of trash, and clean trash after data is saved
#
#	Sequence of events:
#	1. use refreshNamedSessionCustomTracks program to extract list
#		of files that belong to sessions into 'sessionFiles' list
#	2. construct list of files 'alreadySaved' in userdata/ct/
#	3. a 'comm -13' command between those two lists will make a list
#		of trash/ct/ files that should be moved 'saveList'
#	4. walk through that list, if file is found in trash/ct/
#		move it to userdata/ct/ and leave a symlink from trash/ct/
#		to the moved file in userdata/ct/
#		SAFETY FEATURE: only do maxFiles number of files in one run
#		SAFETY FEATURE: hopefully race condition minimized by
#		using '/bin/ln -f -s' to do removal and symlink creation in
#		one command.
#	5. run dbTrash cleaner to remove customTrash database tables
#		that have not been referenced within the expiration time.
#	6. run the 'find/remove' commands on trash/ to eliminate files
#		that have not been referenced within the expiration time.
#		two different time periods used for two classes of files
#	7. again construct list of files already saved in userdata/ct/
#		comm -23 with 'sessionFiles' to leave a list of files
#		that could be completely expired from userdata/ct/ and trash/ct/
#		SAFETY FEATURE: does not actually remove these files, just
#		leaves a list for removal consideration
#	SAFETY FEATURE: since this script has 'set -beEu -o pipefail' it will
#		stop at *any* command that fails.  Therefore, it needs to
#		be called from a script that will verify it completes correctly
#		by verifying the final "SUCCESS" message.

# full cmd path names are sometimes helpful for cron jobs which do not
# have full shell environment, plus these save time with the shell since
# it doesn't have to search the PATH for everything.
export ECHO="/bin/echo -e"
export machName=`uname -n`
export sortCmd="/usr/bin/sort"
export egrepCmd="/bin/egrep"
export awkCmd="/usr/bin/awk"
export cutCmd="/usr/bin/cut"
export binDate="/bin/date"
export dateCmd="${binDate} +%Y-%m-%dT%H:%M:%S"
export rmCmd="/bin/rm -f"
# for testing, the rm command is an echo instead of a real remove
# export rmCmd="${ECHO}"

# userData directory definition must come in from the caller
if [ -z "${userData+x}" ]; then
  echo "trashCleaner.bash: ERROR: no userData defined from monitor caller ?" 1>&2
  exit 255
fi
# location of custom track files in trash
if [ -z "${trashCt+x}" ]; then
  echo "trashCleaner.bash: ERROR: no trashCt defined from monitor caller ?" 1>&2
  exit 255
fi

# location to find other scripts and programs used here
if [ -z "${scriptsDir+x}" ]; then
  echo "trashCleaner.bash: ERROR: no scriptsDir defined from monitor caller ?" 1>&2
  exit 255
fi

# verify lock file definition comes in from the calling monitor program:
if [ -z "${lockFile+x}" ]; then
  echo "trashCleaner.bash: ERROR: no lockFile defined from monitor caller ?" 1>&2
  exit 255
fi
# set hardLinkOK=1 if the userData save area is on the same filesystem
# as the trashCt.  If this is not true, set hardLinkOK=0 to avoid hard links.
# using the 'stat' command to determine if userData and trashCt are
# on the same filesystem
if [ `stat -c "%d" ${userData}` -eq `stat -c "%d" ${trashCt}` ]; then
   export hardLinkOK=1
else
   export hardLinkOK=0
fi

if [ -z "${LOGDIR+x}" ]; then
  echo "trashCleaner.bash: ERROR: no LOGDIR defined from monitor caller ?" 1>&2
  exit 255
fi
# actual trash directory on this machine
if [ -z "${trashDir+x}" ]; then
  echo "trashCleaner.bash: ERROR: no trashDir defined from monitor caller ?" 1>&2
  exit 255
fi
export refreshCommand="${scriptsDir}/refreshNamedSessionCustomTracks -workDir=${trashDir}"
export dbTrash="${scriptsDir}/dbTrash"
# fetch this from cgi-bin/hg.conf
export centralDb=` grep "^central.db=" /usr/local/apache/cgi-bin/hg.conf | awk -F'=' '{print $2}'`
# location of custom track files in userdata area
export userCt="${userData}/ct/${machName}"
# relative path from actual trash/ct/ directory to userdata save area
export relativeUserCt="../../userdata/ct/${machName}"
export userLog="${LOGDIR}/trashCleaner"
export considerRemoval="${userLog}/considerRemoval.txt"
# dateStamp with hour so this could make a new log file each hour
export dateStamp=`${binDate} "+%Y-%m-%dT%H:%M"`
export YYYY=`${binDate} "+%Y"`
export MM=`${binDate} "+%m"`
export logDir="${userLog}/${YYYY}/${MM}"
export dbTrashLog="${logDir}/dbTrash.${dateStamp}.txt"
export trashLog="${logDir}/trash.${YYYY}-${MM}.txt"
export rmLogDir="${userLog}/removed/${YYYY}/${MM}"
# two different time periods for expiration
# completely expendable file in trash can go after one hour
# custom track files expire after 72 hours
export shortTimeHours=1
export longTimeHours=72
export dbTrashTime="${longTimeHours}"    # custom tracks expire time in hours

# Limit the number of files to process into userData at once:
export maxFiles=1000

usage() {
    ${ECHO} "usage:  trashCleaner.bash searchAndDestroy" 1>&2
    ${ECHO} "NOTE: to be used only by trachCleanMonitor.sh - this is not a" 1>&2
    ${ECHO} "script to be used by itself." 1>&2
    ${ECHO} "This script will run when given the argument searchAndDestroy" 1>&2
    ${ECHO} "which is exactly what it will do to the trash files for the" 1>&2
    ${ECHO} "genome browser.  There is no turning back after it gets going." 1>&2
    ${ECHO} "It will move files that belong to sessions from the directory" 1>&2
    ${ECHO} "${trashCt} to ${userCt}" 1>&2
    ${ECHO} "activity logs can be found in ${userLog}" 1>&2
    exit 255
}

if [ $# -lt 1 ]; then
    usage
    exit 255
fi

if [ $1 != "searchAndDestroy" ]; then
    ${ECHO} "ERROR: must supply one argument: searchAndDestroy"
    usage
    exit 255
fi

# the lockFile *must* exist in order to continue since it means
# the monitor script is running and watching us
if [ -s "${lockFile}" ]; then
    ${ECHO} "${machName} cleaner $$ "`$dateCmd` >> "${lockFile}"
else
    ${ECHO} "trashCleaner.bash: lockFile ${lockFile} does not exist" 1>&2
    exit 255
fi

${ECHO} "starting ${machName} cleaner "`$dateCmd`

# this special hg.conf needs read-only access to hgcentral (and ordinary dbs)
#	and it needs read-write access to hgcustom customTrash db
export HGDB_CONF="${scriptsDir}/.hg.${machName}.conf"

export alreadySaved=`/bin/mktemp ${TMPDIR}/alreadySaved.XXXXXX`
/bin/chmod 666 "${alreadySaved}"
export sessionFiles=`/bin/mktemp ${TMPDIR}/sessionFiles.XXXXXX`
/bin/chmod 666 "${sessionFiles}"
export saveList=`/bin/mktemp ${TMPDIR}/saveList.XXXXXX`
/bin/chmod 666 "${saveList}"
export refreshList=`/bin/mktemp ${TMPDIR}/refreshList.XXXXXX`
/bin/chmod 666 "${refreshList}"

# get directories started and read permissions if first time use
if [ ! -d "${userCt}" ]; then
    /bin/mkdir -p "${userCt}"
    /bin/chmod 777 "${userCt}"
    /bin/chmod 777 "${userData}/ct"
fi
if [ ! -d "${logDir}" ]; then
    /bin/mkdir -p "${logDir}"
    /bin/chmod 777 "${userLog}"
    /bin/chmod 777 "${userLog}/${YYYY}"
    /bin/chmod 777 "${logDir}"
fi
if [ ! -d ${rmLogDir} ]; then
    /bin/mkdir -p "${rmLogDir}"
    /bin/chmod 777 "${userLog}/removed"
    /bin/chmod 777 "${userLog}/removed/${YYYY}"
    /bin/chmod 777 "${rmLogDir}"
fi

# if this is first reference to trashLog, start it with header lines
if [ ! -s "${trashLog}" ]; then
    ${ECHO} "# date\t\t${shortTimeHours}-hour\t\t${longTimeHours}-hour\t${shortTimeHours}-hour\t\t${longTimeHours}-hour" > "${trashLog}"
    ${ECHO} "#\t\tKbytes\t\tKbytes\t\tfileCount\t\tfileCount" >> "${trashLog}"
    /bin/chmod 666 "${trashLog}"
fi

# Temporary removal of considerRemoval business:
# This is almost always a no-op unless actual removal is underway
# which is a manual procedure

# ${scriptsDir}/tempRemove.bash > /dev/null 2>&1

# this refreshNamedSessionCustomTracks has the side effect of accessing
#	the trash files to be saved and the hgcustom customTrash database
#	tables to keep them alive.  The database access is necessary to
#	keep it alive, but the accesses to the files is no longer
#	required with this new scheme in place .  This egrep will fail if it
#	matches nothing, hence the match to "^Got " will always work so it
#	won't fail

#  capture entire refreshCommand output for perusal and scanning
${refreshCommand} ${centralDb} -verbose=4 > "${refreshList}" 2>&1
${ECHO} "after refreshCommand ${machName} cleaner "`$dateCmd`
${egrepCmd} "^Got |^Found live custom track: |^setting .*File: |^setting bigDataUrl:" "${refreshList}" | \
    ${egrepCmd} "^Got |trash/ct/" \
	| /bin/sed -e "s#.*trash/ct/##; s# connection to hgcentral.*##" \
	| ${sortCmd} -u > "${sessionFiles}"

# construct a list of already moved files
/usr/bin/find $userCt -type f | /bin/sed -e "s#${userCt}/##" | ${sortCmd} \
	> "${alreadySaved}"

# construct a list of files that should move
/usr/bin/comm -13 "${alreadySaved}" "${sessionFiles}" > "${saveList}"

export filesToDo=`/bin/cat "${saveList}" | /usr/bin/wc -l`

# it appears that an empty list is OK for this foreach() loop
#	it does nothing and is not a failure
export filesDone=0
for F in `/bin/cat "${saveList}"`
do
    let "filesDone += 1"
    if [ $filesDone -gt $maxFiles ]; then
        ${ECHO} "completed maximum of $maxFiles files of $filesToDo to do"
	break
    fi
    if [ "${F}" = "Got" ]; then
	filesDone=`echo $filesDone | ${awkCmd} '{print $1-1}'`
	continue
    fi
    export trashFile="${trashCt}/${F}"
    export absSaveFile="${userCt}/${F}"
    export relSaveFile="${relativeUserCt}/${F}"
    if [ -l "${trashFile}" ]; then
#        ${ECHO} "already symlink: ${trashFile}"
	filesDone=`echo $filesDone | ${awkCmd} '{print $1-1}'`
    elif [ -s "${trashFile}" ]; then
#        ${ECHO} "${trashFile} -> ${absSaveFile}"
	if [ $hardLinkOK -eq 1 ]; then
	    /bin/ln ${trashFile} ${absSaveFile}
	    /bin/ln -f -s ${relSaveFile} ${trashFile}
	else
	    (/bin/cp -p ${trashFile} ${absSaveFile} || /bin/true)
	    /bin/ln -f -s ${relSaveFile} ${trashFile}
            /bin/chown www-data:www-data ${absSaveFile}
	fi
    else
        ${ECHO} "not file or symlink: ${trashFile}"
    fi
done

if [ $filesToDo -gt 1 ]; then
    if [ $filesDone -le $maxFiles ]; then
        ${ECHO} "completed all $filesToDo files (< maxFiles: $maxFiles)"
    fi
fi

${ECHO} "after symLinks done ${machName} cleaner "`$dateCmd`

#############################################################################
# can now clean customTrash tables that are aged out

# XXX the -drop has been removed here for testing, and verbose raised to 4
# XXX from 2

/usr/bin/time -p ${dbTrash} -age=${dbTrashTime} -verbose=4 >> "${dbTrashLog}" 2>&1
/bin/chmod 666 "${dbTrashLog}"
${ECHO} "after dbTrash ${machName} cleaner "`$dateCmd`
/bin/gzip "${dbTrashLog}"

#############################################################################
# find and remove commands here to remove trash files
#  measure trash size before removal

# this df is vastly more efficient then using du
export kbBefore=`/bin/df -k "${trashDir}" | /bin/grep "${trashDir}" | /bin/grep -v "^10" | ${awkCmd} '{print $2}'`

# the du in this message refers to the older method, leave this message
#	the same to keep the log messages consistent
${ECHO} "after shortTime hour du ${machName} cleaner "`$dateCmd`

# expected return from trashMonitor.sh script is the name of the access time
# temporary file: ${TMPDIR}/trash.atime.xxxxxx
# a two column file, column 1 is the last access time in seconds since epoch
# column 2 is the full pathname of a file in trash
export atimeFile=`${scriptsDir}/trashSizeMonitor.sh`

export longTimeSeconds=`${ECHO} ${longTimeHours} | ${awkCmd} '{printf "%d", $1*60*60}'`
export shortTimeSeconds=`${ECHO} ${shortTimeHours} | ${awkCmd} '{printf "%d", $1*60*60}'`
export curTime=`${binDate} "+%s"`
export expireShortTime=`echo $curTime $shortTimeSeconds | ${awkCmd} '{printf "%d", $1-$2}'`
export shortTimeAtimeFile=`echo $atimeFile | /bin/sed -e "s/trash.atime/shortTime/"`
${awkCmd} -v atime=$expireShortTime '$1 < atime' "${atimeFile}" > "${shortTimeAtimeFile}"

# avoiding custom tracks in trash/ct/ trash/hgg/ trash/hgSs/ and trash/udcCache

( echo 'noMatchToNothing'; ${cutCmd} -f2 "${shortTimeAtimeFile}" ) \
    | ${egrepCmd} -v "${trashDir}/ct/|${trashDir}/hgg/|${trashDir}/hgSs/|${trashDir}/udcCache/" \
	| ${sortCmd} > ${TMPDIR}/${machName}.shortTime.list.txt

#  This is the shortTime-hour remove command, shortTime-hour since last access
#       cleaner avoids files in the egrep

/bin/chmod 666 ${TMPDIR}/${machName}.shortTime.list.txt
/bin/cp -p ${TMPDIR}/${machName}.shortTime.list.txt ${rmLogDir}/shortTimeList.${dateStamp}.txt
/bin/gzip ${rmLogDir}/shortTimeList.${dateStamp}.txt
export countShortTime=`${egrepCmd} "noMatchToNothing|^$trashDir" ${TMPDIR}/${machName}.shortTime.list.txt | /bin/sed -e '/noMatchToNothing/d' | /usr/bin/wc -l`
${egrepCmd} "noMatchToNothing|^$trashDir" ${TMPDIR}/${machName}.shortTime.list.txt | /bin/sed -e "/noMatchToNothing/d; s/'/\\\'/" | xargs --no-run-if-empty ${rmCmd}
${ECHO} "after shortTime hour find ${machName} cleaner "`$dateCmd`" $countShortTime"

export kbAfter=`/bin/df -k "${trashDir}" | /bin/grep "${trashDir}" | /bin/grep -v "^10" | ${awkCmd} '{print $2}'`

export shortTimeClean=`echo $kbBefore $kbAfter | ${awkCmd} '{printf "%d", $1-$2}'`
export kbBefore=$kbAfter

#  This is the longTime-hour remove command, longTime-hour since last
#       access cleaner works only on files in the egrep

# longTimeHours hour (long time interval) only expires trash/ct/ trash/hgg/
# trash/hgSs and trash/udcCache files

export curTime=`${binDate} "+%s"`
export longTimeAtimeFile=`echo $atimeFile | /bin/sed -e "s/trash.atime/seventyTwo.hour/"`
export expireLongTime=`echo $curTime $longTimeSeconds | ${awkCmd} '{printf "%d", $1-$2}'`
${awkCmd} -v atime=$expireLongTime '$1 < atime' "${atimeFile}" > "${longTimeAtimeFile}"
/bin/chmod 666 "${longTimeAtimeFile}"
( echo 'noMatchToNothing'; ${cutCmd} -f2 "${longTimeAtimeFile}" ) \
    | ${egrepCmd} "noMatchToNothing|${trashDir}/ct/|${trashDir}/hgg/|${trashDir}/hgSs/|${trashDir}/udcCache/" \
	| ${sortCmd} > ${TMPDIR}/${machName}.longTime.list.txt

/bin/chmod 666 ${TMPDIR}/${machName}.longTime.list.txt
/bin/cp -p ${TMPDIR}/${machName}.longTime.list.txt ${rmLogDir}/longTimeList.${dateStamp}.txt
/bin/gzip ${rmLogDir}/longTimeList.${dateStamp}.txt
export countLongTime=`${egrepCmd} "noMatchToNothing|^$trashDir" ${TMPDIR}/${machName}.longTime.list.txt | /bin/sed -e '/noMatchToNothing/d' | /usr/bin/wc -l`

${egrepCmd} "noMatchToNothing|^$trashDir" ${TMPDIR}/${machName}.longTime.list.txt | /bin/sed -e "/noMatchToNothing/d; s/'/\\\'/" | xargs --no-run-if-empty ${rmCmd}
${ECHO} "after longTime hour find ${machName} cleaner "`$dateCmd`" $countLongTime"

export kbAfter=`/bin/df -k "${trashDir}" | /bin/grep "${trashDir}" | /bin/grep -v "^10" | ${awkCmd} '{print $2}'`
export longTimeClean=`${ECHO} $kbBefore $kbAfter | ${awkCmd} '{printf "%d", $1-$2}'`
${ECHO} "after longTime hour du ${machName} cleaner "`$dateCmd`

${ECHO} "${dateStamp}\t${shortTimeClean}\t${longTimeClean}\t${countShortTime}\t${countLongTime}" \
	>> "${trashLog}"
#
#############################################################################

/usr/bin/find "${userCt}" -type f | /bin/sed -e "s#${userCt}/##" | ${sortCmd} \
	> "${alreadySaved}"

# construct a list of files that could be removed from userCt
/usr/bin/comm -23 "${alreadySaved}" "${sessionFiles}" > "${considerRemoval}"
# XXX did not work under Apache
# /bin/chmod 666 "${considerRemoval}"
${rmCmd} "${saveList}"
${rmCmd} "${alreadySaved}"
${rmCmd} "${sessionFiles}"
/bin/grep VmPeak "${refreshList}"
${rmCmd} "${refreshList}"
${rmCmd} "${shortTimeAtimeFile}" "${longTimeAtimeFile}"
if [ -e "${atimeFile}" ]; then
  ${rmCmd} "${atimeFile}"
fi

${rmCmd} "${lockFile}"
${ECHO} "SUCCESS trash cleaning "`$dateCmd`
sleep 1
exit 0

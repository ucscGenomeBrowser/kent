#!/bin/csh -fe
#
#	trashCleaner.csh - script to save session user data into a special
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
#	SAFETY FEATURE: since this script is 'csh -fe' it will stop at *any*
#		command that fails.  Therefore, it needs to be called from
#		a script that will verify it completes correctly by verifying
#		the final "SUCCESS" message.

set ECHO = "/bin/echo -e"

# set hardLinkOK = 1 if the userData save area is on the same filesystem
# as the trashCt.  If this is not true, set hardLinkOK = 0 to avoid hard links.
set hardLinkOK = 1
set refreshCommand = "/cluster/bin/x86_64/refreshNamedSessionCustomTracks"
set dbTrash = "/cluster/bin/x86_64/dbTrash"
set dbTrashTime = "72"				# time in hours
set centralDb = "hgcentraltest"

set trashDir = "/data/apache/trash"
set trashCt = "${trashDir}/ct"
set userData = "/data/apache/userdata"
set userCt = "${userData}/ct"
set userLog = "${userData}/log"
set considerRemoval = "${userLog}/considerRemoval.txt"
# dateStamp with hour so this could make a new log file each hour
set dateStamp = `date "+%Y-%m-%dT%H"`
set YYYY = `date "+%Y"`
set MM = `date "+%m"`
set logDir = "${userLog}/${YYYY}/${MM}"
set dbTrashLog = "${logDir}/dbTrash.${dateStamp}.txt"
set trashLog = "${logDir}/trash.${YYYY}-${MM}.txt"
# number of minutes for expiration for find commands on trash files
set eightHours = "+480"
set seventyTwoHours = "+4320"
# we do not want to do everything at once.  Limit the number of
# files to process at once:
set maxFiles = 2

if ( $1 != "searchAndDestroy" ) then
    ${ECHO} "usage:  trashCleaner.csh searchAndDestroy"
    ${ECHO} "This script will run when given the argument searchAndDestroy"
    ${ECHO} "which is exactly what it will do to the trash files for the"
    ${ECHO} "genome browser.  There is no turning back after it gets going."
    ${ECHO} "It will move files that belong to sessions from the directory"
    ${ECHO} "${trashCt} to ${userCt}"
    ${ECHO} "activity logs can be found in ${userLog}"
    exit 255
endif

# this special hg.conf needs read-only access to hgcentral (and ordinary dbs)
#	and it needs read-write access to hgcustom customTrash db
setenv HGDB_CONF "/data/apache/userdata/.hg.localhost.conf"

set alreadySaved = `mktemp /data/tmp/ct/alreadySaved.XXXXXX`
chmod 666 "${alreadySaved}"
set sessionFiles = `mktemp /data/tmp/ct/sessionFiles.XXXXXX`
chmod 666 "${sessionFiles}"
set saveList = `mktemp /data/tmp/ct/saveList.XXXXXX`
chmod 666 "${saveList}"
# set refreshList = `mktemp /data/tmp/ct/refreshList.XXXXXX`
# chmod 666 "${refreshList}"

# get directories started and read permissions if first time use
if ( ! -d "${userCt}" ) then
    mkdir -p "${userCt}"
    chmod 755 "${userCt}"
endif
if ( ! -d "${logDir}" ) then
    mkdir -p "${logDir}"
    chmod 755 "${userLog}"
    chmod 755 "${userLog}/${YYYY}"
    chmod 755 "${logDir}"
endif

# if this is first reference to trashLog, start it with header lines
if ( ! -s "${trashLog}" ) then
    ${ECHO} "# date\t\t8-hour\t\t72-hour" > "${trashLog}"
    ${ECHO} "#\t\tKbytes\t\tKbytes" >> "${trashLog}"
    chmod 666 "${trashLog}"
endif

#  XXX debugging, capture entire refreshCommand output for perusal
# ${refreshCommand} ${centralDb} -verbose=4 >& "${refreshList}"

# this refreshNamedSessionCustomTracks has the side effect of accessing
#	the trash files to be saved and the hgcustom customTrash database
#	tables to keep them alive.  The database access is necessary to
#	keep it alive, but the accesses to the files is no longer
#	required with this new scheme in place .  This egrep will fail if it
#	matches nothing, hence the match to "^Got " will always work so it
#	won't fail

${refreshCommand} ${centralDb} -verbose=4 |& \
egrep "^Got |^Found live custom track: |^setting .*File: |^setting bigDataUrl:" |& \
    egrep "^Got |trash/ct/" \
	| sed -e "s#.*trash/ct/##; s# connection to hgcentral.*##" \
	| sort -u > "${sessionFiles}"

# construct a list of already moved files
# XXX what if this finds nothing ?  Is it a failure ?
find $userCt -type f | sed -e "s#${userCt}/##" | sort \
	> "${alreadySaved}"

# construct a list of files that should move
# XXX what if this comm produces nothing, is it a failure ?
comm -13 "${alreadySaved}" "${sessionFiles}" > "${saveList}"

set filesToDo =  `cat "${saveList}" | wc -l`

# it appears that an empty list is OK for this foreach() loop
#	it does nothing and is not a failure
foreach F (`cat "${saveList}"`)
    @ filesDone++
    if ( $filesDone > $maxFiles ) then
        ${ECHO} "completed maximum of $maxFiles files of $filesToDo to do"
	break
    endif
    if ( "${F}" == "Got" ) then
	@ filesDone--
	continue
    endif
    set trashFile = "${trashCt}/${F}"
    set saveFile = "${userCt}/${F}"
    if ( -l "${trashFile}" ) then
        ${ECHO} "already symlink: ${trashFile}"
	@ filesDone--
    else if ( -s "${trashFile}" ) then
        ${ECHO} "${trashFile} -> ${saveFile}"
	if ( $hardLinkOK == 1 ) then
	    ln ${trashFile} ${saveFile}
	    ln -f -s ${saveFile} ${trashFile}
	else
	    cp -p ${trashFile} ${saveFile}
	    ln -f -s ${saveFile} ${trashFile}
	endif
    else
        ${ECHO} "not file or symlink: ${trashFile}"
    endif
end

if ( $filesToDo > 0 ) then
    if ( $filesDone <= $maxFiles ) then
        ${ECHO} "completed all $filesToDo files (< maxFiles: $maxFiles)"
    endif
endif

#############################################################################
# can now clean customTrash tables that are aged out
${dbTrash} -extFile -extDel -drop -age=${dbTrashTime} \
	-verbose=2 >>& ${dbTrashLog}

#############################################################################
# find and remove commands here to remove trash files
#  measure trash size before removal
set kbBefore = \
`du --apparent-size -ksc "${trashDir}" | grep "${trashDir}" | awk '{print $1}'`

#  This is the 8-hour remove command, note the find works directly on
#  the specified full path trashDir.  This 8-hour since last access
#       cleaner avoids files in trash/ct/ and trash/hgSs/ to leave
#	custom tracks existing.

find ${trashDir} \! \( -regex "${trashDir}/ct/.*" \
	-or -regex "${trashDir}/hgSs/.*" \) -type f -amin ${eightHours} \
	-exec rm -f {} \;

set kbAfter = \
`du --apparent-size -ksc "${trashDir}" | grep "${trashDir}" | awk '{print $1}'`
set eightHourClean = `echo $kbBefore $kbAfter | awk '{printf "%d", $1-$2}'`
set kbBefore = $kbAfter

#  This is the 72-hour remove command, note the find works directly on the
#       specified full path TRASHDIR.  This 72-hour since last access cleaner
#       works only on files in trash/ct/ and trash/hgSs/

find ${trashDir} \( -regex "${trashDir}/ct/.*" \
	-or -regex "${trashDir}/hgSs/.*" \) -type f -amin ${seventyTwoHours} \
	-exec rm -f {} \;

set kbAfter = \
`du --apparent-size -ksc "${trashDir}" | grep "${trashDir}" | awk '{print $1}'`
set seventyTwoHourClean = `echo $kbBefore $kbAfter | awk '{printf "%d", $1-$2}'`

${ECHO} "${dateStamp}\t${eightHourClean}\t${seventyTwoHourClean}" \
	>> "${trashLog}"
#
#############################################################################

# XXX is this find OK if it finds nothing ?
find "${userCt}" -type f | sed -e "s#${userCt}/##" | sort \
	> "${alreadySaved}"

# construct a list of files that could be removed from userCt
# XXX what if this comm produces nothing, is it a failure ?
comm -23 "${alreadySaved}" "${sessionFiles}" > "${considerRemoval}"
chmod 666 "${considerRemoval}"
${ECHO} "candidates for removal, from sessions removed or files replaced:"
cat "${considerRemoval}"
rm -f "${saveList}"
rm -f "${alreadySaved}"
rm -f "${sessionFiles}"
# rm -f "${refreshList}"

${ECHO} "SUCCESS trash cleaning "`date`
exit 0

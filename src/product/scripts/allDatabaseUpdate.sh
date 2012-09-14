#!/bin/bash
#
#	$Id: allDatabaseUpdate.sh,v 1.2 2010/03/31 18:18:31 hiram Exp $
#
usage() {
    echo "usage: allDatabaseUpdate.sh <browserEnvironment.txt> \\"
    echo -e "\t<minimal.db.list> <full.db.update.list> <full.gbdb.list>"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "This script will update all files in /gbdb/ and"
    echo "  in goldenPath/*/database and reload all the databases with any"
    echo "  new tables that show up in goldenPath/*/database"
    echo "The three lists will control how much updates in gbdb and goldenPath:"
    echo "<minimal.db.list> - minimal set of files in gbdb and goldenPath"
    echo "<full.db.update.list> - goldenPath db list for full db update"
    echo "<full.gbdb.list> - gbdb db list for full file update"
    exit 255
}

##########################################################################
# Minimal argument checking and use of the specified include file

if [ $# -ne 4 ]; then
    usage
fi

export includeFile=$1
if [ "X${includeFile}Y" = "XY" ]; then
    usage
fi

if [ -f "${includeFile}" ]; then
    . "${includeFile}"
else
    echo "ERROR: allDatabaseUpdate.sh: can not find ${includeFile}"
    usage
fi

export minimalDbList=$2
export fullDbUpdateList=$3
export fullGbdbList=$4
if [ ! -s "${minimalDbList}" ]; then
    echo "ERROR: allDatabaseUpdate.sh: can not read minimal.db.list: '${minimalDbList}'"
    usage
fi
if [ ! -s "${fullDbUpdateList}" ]; then
    echo "ERROR: allDatabaseUpdate.sh: can not read full.db.update.list: '${fullDbUpdateList}'"
    usage
fi
if [ ! -s "${fullGbdbList}" ]; then
    echo "ERROR: allDatabaseUpdate.sh: can not read full.gbdb.list: '${fullGbdbList}'"
    usage
fi

exit 0

# before moving forward, this script needs to find out where it is being
#	run from to use that location to find all the other scripts
export HERE=`pwd`
export TOOLDIR=`dirname $0`
# assume relative reference from HERE, fixup if absolute
export TOOLSCRIPTS="${HERE}/${TOOLDIR}"

if [[ "${TOOLDIR}" =~ "^/" ]]; then	# absolute reference to this script
    TOOLSCRIPTS="${TOOLDIR}"
fi

runAll() {
${TOOLSCRIPTS}/fetchMinimalGbdb.sh "${minimalDbList}"
${TOOLSCRIPTS}/fetchFullGbdb.sh "${fullGbdbList}"
${TOOLSCRIPTS}/fetchMinimalGoldenPath.sh "${minimalDbList}"
${TOOLSCRIPTS}/fetchFullGoldenPath.sh "${fullDbUpdateList}"
echo "rsync updates done, log files are in"
echo "${LOGDIR}/gbdb/ and ${LOGDIR}/goldenPath/"
export DS=`date "+%Y-%m-%d"`
export GPLOGS="${LOGDIR}/goldenPath/${DS}"
${TOOLSCRIPTS}/loadUpdateGoldenPath.sh "${fullDbUpdateList}"
echo "database load updates done, log files are in"
echo "${GPLOGS}"
ERRCOUNT=`grep ERROR ${GPLOGS}/*.load.* | wc -l`
if [ "${ERRCOUNT}" -eq 0 ]; then
    echo "goldenPath update and database load appears to be OK"
    echo "it should be OK to run the tagGoldenPath.sh script:"
    echo "${TOOLSCRIPTS}/tagGoldenPath.sh"
else
    echo "ERROR: errors in loading database tables.  Check the logs in"
    echo "${GPLOGS}/*.load.* to check the problems before continuing"
    echo "with the tagGoldenPath.sh script.  grep ERROR those files:"
    grep ERROR ${GPLOGS}/*.load.*
fi
}

runAll 2>&1 | mail -s 'genome update report' "${LOGNAME}"

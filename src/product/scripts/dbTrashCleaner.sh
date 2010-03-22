#!/bin/sh
#
#	$Id: dbTrashCleaner.sh,v 1.1.2.1 2010/03/22 18:32:53 hiram Exp $
#
usage() {
    echo "usage: dbTrashCleaner.sh <browserEnvironment.txt>"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "This script should be run periodically to clean out unused tables"
    echo "  in the customTrash database.  Logs of cleaned data statistics"
    echo "  can be found in your specified LOGDIR/customTrash/YYYY/MM"
    echo "Please note comments in this script about the setting of"
    echo "  the HGDB_CONF path name."
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
    echo "ERROR: dbTrashCleaner.sh: can not find ${includeFile}"
    usage
fi

DS=`date "+%Y-%m-%dT%H"`
YYYY=`date "+%Y"`
MM=`date "+%m"`
export DS YYYY MM

D="${LOGDIR}/customTrash/${YYYY}/${MM}"
/bin/mkdir -p "${D}"
LOGFILE="$D/${DS}.txt"
export LOGFILE
#  If your customTrash database is on a different server than your
#	primary database server, you will need a special hg.conf
#	file in your home directory.  For example:
#  HGDB_CONF=$HOME/.ctDb.hg.conf
#  Assuming your customTrash database is on the same MySQL server as
#	all your other databases and your ordinary .hg.conf file will
#	allow read/write access to customTrash, this setting is sufficient:
HGDB_CONF=$HOME/.hg.conf
export HGDB_CONF

# 72 hour cleaning policy on customTrash tables:
${KENTBIN}/$MACHTYPE/dbTrash -age=72 -drop -verbose=2 > ${LOGFILE} 2>&1

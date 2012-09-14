#!/bin/bash
#
#	$Id: fetchFullGbdb.sh,v 1.2 2010/03/19 16:19:34 hiram Exp $
#
usage() {
    echo "usage: fetchFullGbdb.sh <browserEnvironment.txt> <db.Ids>"
    echo "where: <db.Ids> is either a file listing databases to fetch"
    echo "       or a simple argument list of databases to fetch"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "This script performs the job of running rsync to update your"
    echo "  /gbdb/<db>/ directories with a *all* files"
    echo "  for those genome browsers."
    echo "list example:"
    echo "  fetchFullGbdb.sh browserEnvironment.txt full.db.list"
    echo "single db example:"
    echo "  fetchFullGbdb.sh browserEnvironment.txt hg19"
    exit 255
}

##########################################################################
# Minimal argument checking and use of the specified include file

if [ $# -lt 2 ]; then
    usage
fi

export includeFile=$1
if [ "X${includeFile}Y" = "XY" ]; then
    usage
fi

if [ -f "${includeFile}" ]; then
    . "${includeFile}"
else
    echo "ERROR: fetchFullGbdb.sh: can not find ${includeFile}"
    usage
fi

##########################################################################
# fetchOne() - given one database name, fetch all /gbdb/<db>/ files
fetchOne() {
DB=$1
echo "working on ${GBDB}/$DB"
time0=`date "+%s"`
echo -n "starting at: "
date -u
${RSYNC} --partial --stats \
	${HGDOWNLOAD}/gbdb/${DB}/ ${GBDB}/${DB}/
time1=`date "+%s"`
elapsedTime=`echo $time1 $time0 | awk '{print $1-$2}'`
echo -n "finished at: "
date -u
echo "elapsed seconds: $elapsedTime"
du --apparent-size -hsc ${GBDB}/${DB} | grep -v total
}

##########################################################################
# main business starting here

DS=`date "+%Y-%m-%d"`
export DS
FETCHLOG="${LOGDIR}/gbdb/${DS}"
export FETCHLOG
mkdir -p ${FETCHLOG}

# get rid of the browserEnvironment.txt argument
shift

for ARG in $*
do
    if [ -s "${ARG}" ]; then
	for D in `grep -v "^#" "${ARG}"`
	do
	    fetchOne ${D} > ${FETCHLOG}/${D}.fetch.$DS 2>&1
	done
	exit 0
    else
	fetchOne ${ARG} > ${FETCHLOG}/${ARG}.fetch.$DS 2>&1
    fi
done

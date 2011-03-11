#!/bin/sh
#
#	$Id: fetchMinimalGoldenPath.sh,v 1.2 2010/03/19 16:19:34 hiram Exp $
#
usage() {
    echo "usage: fetchMinimalGoldenPath.sh <browserEnvironment.txt> <db.Ids>"
    echo "where: <db.Ids> is either a file listing databases to fetch"
    echo "       or a simple argument list of databases to fetch"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "This script performs the job of running rsync to update your"
    echo "  goldenPath/database directories with a *minimal* set of files"
    echo "  to run those genome browsers."
    echo "list example:"
    echo "  fetchMinimalGoldenPath.sh browserEnvironment.txt minimal.db.list"
    echo "single db example:"
    echo "  fetchMinimalGoldenPath.sh browserEnvironment.txt hg19"
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
    echo "ERROR: fetchMinimalGoldenPath.sh: can not find ${includeFile}"
    usage
fi

##########################################################################
# fetchOne() - given one database name, fetch goldenPath/<db>/database/ files
fetchOne() {
DB=$1
echo "working on ${GOLD}/$DB/database"
time0=`date "+%s"`
echo -n "starting at: "
date -u
mkdir -p ${GOLD}/${DB}/database
${RSYNC} --partial --stats \
    --include="/chromInfo.*" --include="/*gold*" --include="/*gap.txt.gz" \
    --include="/grp.*" --include="/metaDb.*" --include="/trackDb.*" \
    --include="/hgFindSpec.*" --include="/*gap.sql" --exclude="*" \
    ${HGDOWNLOAD}/goldenPath/${DB}/database/ \
     ${GOLD}/${DB}/database/
time1=`date "+%s"`
elapsedTime=`echo $time1 $time0 | awk '{print $1-$2}'`
echo -n "finished at: "
date -u
echo "elapsed seconds: $elapsedTime"
du --apparent-size -hsc ${GOLD}/${DB}/database | grep -v total
}

##########################################################################
# main business starting here

DS=`date "+%Y-%m-%d"`
export DS
FETCHLOG="${LOGDIR}/goldenPath/${DS}"
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

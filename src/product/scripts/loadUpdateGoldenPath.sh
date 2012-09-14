#!/bin/bash
#
#	$Id: loadUpdateGoldenPath.sh,v 1.2 2010/03/31 18:18:31 hiram Exp $
#
usage() {
    echo "usage: loadUpdateGoldenPath.sh <browserEnvironment.txt> <db.Ids>"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "<db.Ids> is either a file listing databases to load"
    echo "       or a simple argument list of databases to load"
    echo "This script will use the lastTimeStamp files from"
    echo "    previous updates in goldenPath/*/database to determine tables"
    echo "    to load due to rsync updates in goldenPath/<db>/database/"
    echo "list example: loadUpdateGoldenPath.sh insect.list"
    echo "  db example: loadUpdateGoldenPath.sh dm3"
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
    echo "ERROR: loadUpdateGoldenPath.sh: can not find ${includeFile}"
    usage
fi

# get rid of the browserEnvironment.txt argument
shift

##########################################################################
loadTbl() {
DB=$1
TABLE=$2
SQL=${TABLE}.sql
if [ -s "${SQL}" ]; then
    if [ -s "${TABLE}.txt.gz" ]; then
	echo "loading ${TABLE}.${DB}"
	${HGSQL} -e "drop table ${TABLE};" ${DB} > /dev/null 2> /dev/null
	${HGSQL} "${DB}" < "${SQL}"
	zcat "${TABLE}.txt.gz" | ${HGSQL} -e \
	    "load data local infile \"/dev/stdin\" into table ${TABLE};" ${DB}
    else
	echo "ERROR: can not find: ${DB}/database/${TABLE}.txt.gz"
    fi
else
    echo "ERROR: can not find: ${DB}/database/${TABLE}.sql"
fi
}

##########################################################################
# main business starting here

DS=`date "+%Y-%m-%d"`
export DS
LOADLOG="${LOGDIR}/goldenPath/${DS}"
export LOADLOG
mkdir -p ${LOADLOG}

for ARG in $*
do
    if [ -s "${ARG}" ]; then
	for D in `grep -v "^#" "${ARG}"`
	do
	    echo ${D}
	done
    else
	echo ${ARG}
    fi
done | while read D
do
    if [ ! -d ${GOLD}/${D}/database ]; then
	echo "ERROR: ${GOLD}/${D}/database/ directory does not exist ?"
	exit 255
    fi
    cd ${GOLD}/${D}/database
    HERE=`pwd`
    DB=`dirname ${HERE}`
    DB=`basename ${DB}`
    TBL_COUNT=`${HGSQL} -N -e "show tables;" ${DB} | wc -l`
    if [ "${TBL_COUNT}" -gt 3 ]; then
	WC=`ls -ogrt | grep "^-" | grep lastTimeStamp | tail -1 | wc -l`
	if [ "${WC}" -ne 1 ]; then
	    echo "ERROR: can find no lastTimeStamp in ${GOLD}/${D}/database" \
		> ${LOADLOG}/${DB}.load.${DS}
	else
	    REF=`ls -ogrt | grep "^-" | grep lastTimeStamp | tail -1 \
		| awk '{print $NF}'`
	    echo "${DB} ${REF}" > ${LOADLOG}/${DB}.load.${DS}
	    find . -type f -newer "${REF}" | egrep ".sql$|.txt.gz$" \
		| sed -e "s/.sql$//; s/.txt.gz$//; s#^./##;" | sort -u \
	    | while read TBL
	    do
		loadTbl "${DB}" "${TBL}"
	    done >> ${LOADLOG}/${DB}.load.${DS}
	fi
    else
	echo \
"ERROR: database ${DB} has too few tables (${TBL_COUNT}) to begin with" \
	> ${LOADLOG}/${DB}.load.${DS}
    fi
    cd "${GOLD}"
done

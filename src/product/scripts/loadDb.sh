#!/bin/bash
#
#	$Id: loadDb.sh,v 1.2 2010/03/31 18:18:31 hiram Exp $
#
usage() {
    echo "usage: loadDb.sh <browserEnvironment.txt> <DB>"
    echo "where <DB> is one of the directories in goldenPath/<DB>/database/"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "This script performs the job of completely loading"
    echo "  the specified database from the goldenPath database MySQL"
    echo "  text dumps.  This script will *not* run if the database already"
    echo "  exists and has tables.  It will load an empty existing database,"
    echo "  or create the database if it does not exist."
    echo "single db example:"
    echo "  loadDb.sh browserEnvironment.txt hg19"
    exit 255
}

##########################################################################
# Minimal argument checking and use of the specified include file

if [ $# -ne 2 ]; then
    usage
fi

export includeFile=$1
if [ "X${includeFile}Y" = "XY" ]; then
    usage
fi

if [ -f "${includeFile}" ]; then
    . "${includeFile}"
else
    echo "ERROR: loadDb.sh: can not find ${includeFile}"
    usage
fi

DB=$2
export DB

SRC=${GOLD}/${DB}/database
export SRC

if [ ! -d "${SRC}" ]; then
    echo "ERROR: can not find database directory:"
    echo ${SRC}
    exit 255
fi

TC=`${HGSQL} -N -e "show tables;" ${DB} 2> /dev/null | wc -l`
if [ "${TC}" -ne 0 ]; then
    echo "ERROR: database ${DB} already exists with tables."
    echo "this is only for new databases."
    exit 255
fi

${HGSQL} -e "create database ${DB};" mysql 2> /dev/null
cd "${SRC}"
for SQL in *.sql
do
    T_NAME=${SQL%%.sql}
    echo "loading ${T_NAME}.${DB}"
    if [ -s "${T_NAME}.txt.gz" ]; then
	${HGSQL} -e "drop table ${T_NAME};" ${DB} > /dev/null 2> /dev/null
	grep -v "^----------------------" "${SQL}" | ${HGSQL} "${DB}"
	gunzip -c "${T_NAME}.txt.gz" | ${HGSQL} --local-infile=1 -e \
            "load data local infile \"/dev/stdin\" into table ${T_NAME};" ${DB}
    else
	echo "ERROR: can not find: ${T_NAME}.txt.gz"
    fi
done

#!/bin/sh
#
#	mkSwissProtDB.sh
#	- currently no arguments but it should be modified to take an
#	- argument of a data stamp instead of generating one below since
#	- you want to have consistent date stamps for this swissprot and
#	- the subsequence proteins database
#
#	This script could be improved to do error checking for each step.
#
#	Thu Nov 20 11:31:51 PST 2003 - Created - Hiram
#
#	"$Id: mkSwissProtDB.sh,v 1.1 2003/11/20 19:37:44 hiram Exp $"

TOP=/cluster/data/swissprot
export TOP
cd ${TOP}

type spToDb > /dev/null 2> /dev/null

if [ "$?" -ne 0 ]; then
    echo "ERROR: can not find required program: spToDb"
    echo -e "\tYou may need to build it in ~/kent/src/hg/protein/spToDb"
    exit 255
fi

MACHINE=`uname -n`

if [ ${MACHINE} != "eieio" -a ${MACHINE} != "hgwdev" ]; then
    echo "ERROR: must run this script on eieio or hgwdev.  This is: ${MACHINE}"
    exit 255
fi

DATE=`date "+%y%m%d"`
SP="${DATE}"
SPDB="sp${DATE}"
export SP SPDB

echo "Creating Db: ${SP}"

if [ ${MACHINE} = "eieio" ]; then

    if [ -d "${SP}" ]; then
	echo "WARNING: ${SP} already exists."
	echo -e "Do you want to try to use the data here ? (ynq) \c"
	read YN
	if [ "${YN}" = "Y" -o "${YN}" = "y" ]; then
	    echo "working with current data in ${SP}"
	else
	    echo "Will not recreate at this time."
	    exit 255
	fi
    fi


    echo mkdir -p ./${SP}
    mkdir -p ./${SP}
    cd ./${SP}
    mkdir -p ./build
    cd ./build
    for db in sprot trembl trembl_new
    do
	if [ ! -f ${db}.dat.gz ]; then
		wget ftp://us.expasy.org/databases/sp_tr_nrdb/${db}.dat.gz
	fi
    done
    zcat *.dat.gz | spToDb stdin ../tabFiles

else
    if [ ! -d ${TOP}/${DATE}/tabFiles ]; then
	echo "ERROR: ${TOP}/tabFiles does not exist."
	echo -e "\tRun this first on eieio to fetch the data."
	exit 255
    fi

    if [ ! -f ~/kent/src/hg/protein/spToDb/spDb.sql ]; then
	echo "ERROR: can not find ~/kent/src/hg/protein/spToDb/spDb.sql"
	echo "\tto create the database.  Update your source tree."
	exit 255
    fi

    echo "creating the database ${SPDB}"
    EXISTS=`hgsql -e "show tables;" ${SPDB} 2> /dev/null | wc -l`
    if [ "${EXISTS}" -gt 1 ]; then
	echo "ERROR: database ${SPDB} already exists"
	echo -e "\t to drop: hgsql -e 'drop database ${SPDB};' ${SPDB}"
	exit 255
    fi
    hgsql -e "create database ${SPDB}" proteins072003
    hgsql ${SPDB} < ~/kent/src/hg/protein/spToDb/spDb.sql
    cd ${TOP}/${DATE}/tabFiles
    for i in *.txt
    do
	TBL=${i%.txt}
	echo "importing table: ${TBL}"
	echo hgsql -e "load data local infile '${i}' into table ${TBL};" ${SPDB}
	hgsql -e "load data local infile \"${i}\" into table ${TBL};" ${SPDB}
    done

fi

exit 0

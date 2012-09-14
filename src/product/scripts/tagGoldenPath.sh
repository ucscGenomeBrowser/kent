#!/bin/bash
#
#	$Id: tagGoldenPath.sh,v 1.2 2010/03/31 18:18:31 hiram Exp $
#
usage() {
    echo "usage: tagGoldenPath.sh <browserEnvironment.txt>"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "This script will check the date/time stamps on downloaded files in"
    echo "goldenPath/*/database and touch a lastUpdate file."
    echo "This assumes everything in those database directories has"
    echo "been loaded successfully into the database.  This lastUpdate time"
    echo "stamp will be used to determine what is loaded the next time those"
    echo "directories are updated with the script: loadUpdateGoldenPath.sh."
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
    echo "ERROR: loadUpdateGoldenPath.sh: can not find ${includeFile}"
    usage
fi

DS=`date "+%Y-%m-%dT%H"`
export DS

cd "${GOLD}"
testHere=`pwd`
if [ "X${testHere}Y" != "X${GOLD}Y" ]; then
    echo "ERROR: tagGoldenPath.sh failed to chdir to ${GOLD}"
    exit 255;
fi
find . -type d -name database | while read D
do
    cd "${D}"
    WC=`ls -ogrt | grep "^-" | grep -v lastTimeStamp | tail -1 | wc -l`
    if [ "${WC}" -ne 1 ]; then
	echo "WARNING: empty directory ${D}"
    else
	REF=`ls -ogrt | grep "^-" | grep -v lastTimeStamp | tail -1 \
		| awk '{print $NF}'`
	touch --reference=${REF} lastTimeStamp.${DS}
	echo "${D}/${REF}"
    fi
    cd "${GOLD}"
done

WC=`find . -type d ! -name database | while read D
do
    ls -ld ${D}/* 2> /dev/null | grep -v "^d" | egrep -v "proteins0|proteinDB/README.txt"
done | wc -l`

if [ "${WC}" -ne 0 ]; then
	echo "#  found ${WC} stray files elsewhere, please check these"
	echo "#  in ${GOLD}"
	find . -type d ! -name database | while read D
do
    ls -ld ${D}/* 2> /dev/null | grep -v "^d" | egrep -v "proteins0|proteinDB/README.txt"
done
fi

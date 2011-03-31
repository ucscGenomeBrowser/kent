#!/bin/sh
#
#	$Id: updateHtml.sh,v 1.2 2010/03/31 18:18:32 hiram Exp $
#
usage() {
    echo "usage: updateHtml.sh <browserEnvironment.txt>"
    echo "The browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "  There should be an example template to start with in the"
    echo "  directory with these scripts."
    echo "This script will fetch the static HTML hierarchy from UCSC"
    echo "  into your specified DOCUMENTROOT from the browserEnvironment.txt file."
    echo "It specifically ignores all encode directories and the trash directory."
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
    echo "ERROR: updateHtml.sh: can not find ${includeFile}"
    usage
fi

export DS=`date "+%Y-%m-%d"`
export FETCHLOG="${LOGDIR}/htdocs/update.${DS}"
mkdir -p "${LOGDIR}/htdocs"

echo "#    ${RSYNC} --stats --exclude=\"encode\" --exclude=\"trash\" --exclude=\"lost+found/\" --exclude=\"ENCODE/\" --exclude=\"encodeDCC/\" ${HGDOWNLOAD}/htdocs/ ${DOCUMENTROOT}/" > ${FETCHLOG}
${RSYNC} --stats --exclude="encode" --exclude="trash" \
	--exclude="lost+found/" --exclude="ENCODE/" --exclude="encodeDCC/" \
	${HGDOWNLOAD}/htdocs/ ${DOCUMENTROOT}/ >> ${FETCHLOG} 2>&1
${RSYNC} --stats --delete --max-delete=20 --exclude="encode" --exclude="trash" \
	--exclude="lost+found/" --exclude="ENCODE/" --exclude="encodeDCC/" \
	${HGDOWNLOAD}/htdocs/js/ ${DOCUMENTROOT}/js/ >> ${FETCHLOG} 2>&1
${RSYNC} --stats --delete --max-delete=20 --exclude="encode" --exclude="trash" \
	--exclude="lost+found/" --exclude="ENCODE/" --exclude="encodeDCC/" \
	${HGDOWNLOAD}/htdocs/style/ ${DOCUMENTROOT}/style/ >> ${FETCHLOG} 2>&1

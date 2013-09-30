#!/bin/bash
#
#	$Id: updateHtml.sh,v 1.2 2010/03/31 18:18:32 hiram Exp $
#
usage() {
    echo "usage: updateHtml.sh <browserEnvironment.txt>" 1>&2
    echo "The browserEnvironment.txt file contains definitions of how" 1>&2
    echo "  these scripts behave in your local environment." 1>&2
    echo "  There should be an example template to start with in the" 1>&2
    echo "  directory with these scripts." 1>&2
    echo "This script will fetch the static HTML hierarchy from UCSC" 1>&2
    echo "  into your specified DOCUMENTROOT from the browserEnvironment.txt file." 1>&2
    echo "It specifically ignores the trash directory." 1>&2
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
    echo "ERROR: updateHtml.sh: can not find ${includeFile}" 1>&2
    usage
fi

export errCount=0
if [ ! -d "${BROWSERHOME}" ]; then
    echo "ERROR: BROWSERHOME directory does not exist: ${BROWSERHOME}" 1>&2
    errCount=`echo $errCount | awk '{print $1+1}'`
fi
if [ ! -d "${CGI_BIN}" ]; then
    echo "ERROR: CGI_BIN directory does not exist: ${CGI_BIN}" 1>&2
    errCount=`echo $errCount | awk '{print $1+1}'`
fi

if [ $errCount -gt 0 ]; then
    echo "ERROR: check on the existence of the mentioned directories" 1>&2
    exit 255
fi

export DS=`date "+%Y-%m-%d"`
export FETCHLOG="${LOGDIR}/htdocs/update.${DS}"
mkdir -p "${LOGDIR}/htdocs"
echo "NOTE: logfile for rsync result is in '${FETCHLOG}'" 1>&2

echo "#    ${RSYNC} --stats --exclude=\"trash\" --exclude=\"lost+found/\" ${HGDOWNLOAD}/htdocs/ ${DOCUMENTROOT}/" > ${FETCHLOG}
${RSYNC} --stats --exclude="trash" --exclude="visiGene" \
	--exclude="lost+found/" \
	${HGDOWNLOAD}/htdocs/ ${DOCUMENTROOT}/ >> ${FETCHLOG} 2>&1
${RSYNC} --stats --delete --max-delete=20 --exclude="trash" \
	--exclude="lost+found/" \
	${HGDOWNLOAD}/htdocs/js/ ${DOCUMENTROOT}/js/ >> ${FETCHLOG} 2>&1
${RSYNC} --stats --delete --max-delete=20 --exclude="trash" \
	--exclude="lost+found/" \
	${HGDOWNLOAD}/htdocs/style/ ${DOCUMENTROOT}/style/ >> ${FETCHLOG} 2>&1
${RSYNC} --stats \
        ${HGDOWNLOAD}/cgi-bin/encode/ ${CGI_BIN}/encode/ >> ${FETCHLOG} 2>&1

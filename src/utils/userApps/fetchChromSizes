#!/bin/sh

# fetchChromSizes - script to grab chrom.sizes from UCSC via either of:
#	mysql, wget or ftp

# "$Id: fetchChromSizes,v 1.2 2009/03/26 18:40:09 hiram Exp $"

usage() {
    echo "usage: fetchChromSizes <db> > <db>.chrom.sizes"
    echo "   used to fetch chrom.sizes information from UCSC for the given <db>"
    echo "<db> - name of UCSC database, e.g.: hg18, mm9, etc ..."
    echo ""
    echo "This script expects to find one of the following commands:"
    echo "   wget, mysql, or ftp in order to fetch information from UCSC."
    echo "Route the output to the file <db>.chrom.sizes as indicated above."
    echo ""
    echo "Example:   fetchChromSizes hg18 > hg18.chrom.sizes"
    exit 255
}

DB=$1
export DB
if [ -z "${DB}" ]; then
    usage
fi

WGET=`type wget 2> /dev/null | sed -e "s/.* is //"`
MYSQL=`type mysql 2> /dev/null | sed -e "s/.* is //"`
FTP=`type ftp 2> /dev/null | sed -e "s/.* is //"`

if [ -z "${WGET}" -a -z "${MYSQL}" -a -z "${FTP}" ]; then
    echo "ERROR: can not find any of the commands: wget mysql or ftp"
    usage
fi

DONE=0
export DONE
if [ -n "${MYSQL}" ]; then
    echo "INFO: trying MySQL ${MYSQL} for database ${DB}" 1>&2
    ${MYSQL} -N --user=genome --host=genome-mysql.cse.ucsc.edu -A -e \
	"SELECT chrom,size FROM chromInfo ORDER BY size DESC;" ${DB} 
    if [ $? -ne 0 ]; then
	echo "WARNING: mysql command failed" 1>&2
    else
	DONE=1
    fi
fi

if [ "${DONE}" -eq 1 ]; then
    exit 0
fi

if [ -n "${WGET}" ]; then
    echo "INFO: trying WGET ${WGET} for database ${DB}" 1>&2
    tmpResult=chromInfoTemp.$$.gz
    ${WGET} \
"ftp://hgdownload.cse.ucsc.edu/goldenPath/${DB}/database/chromInfo.txt.gz" \
	-O ${tmpResult} 2> /dev/null
    if [ $? -ne 0 -o ! -s "${tmpResult}" ]; then
	echo "WARNING: wget command failed" 1>&2
	rm -f "${tmpResult}"
    else
	zcat ${tmpResult} 2> /dev/null | cut -f1,2 | sort -k2nr
	rm -f "${tmpResult}"
	DONE=1
    fi
fi

if [ "${DONE}" -eq 1 ]; then
    exit 0
fi

if [ -n "${FTP}" ]; then
    echo "INFO: trying FTP ${FTP} for database ${DB}" 1>&2
    tmpFtpRsp=fetchTemp.$$
    echo "user anonymous fetchChromSizes@" > ${tmpFtpRsp}
    echo "cd goldenPath/${DB}/database" >> ${tmpFtpRsp}
    echo "get chromInfo.txt.gz ${tmpResult}" >> ${tmpFtpRsp}
    echo "bye" >> ${tmpFtpRsp}
    ${FTP} -u -n -i hgdownload.cse.ucsc.edu < ${tmpFtpRsp} > /dev/null 2>&1
    if [ $? -ne 0 -o ! -s "${tmpResult}" ]; then
	echo "ERROR: ftp command failed" 1>&2
	rm -f "${tmpFtpRsp}" "${tmpResult}"
    else
	zcat ${tmpResult} | cut -f1,2 | sort -k2nr
	rm -f "${tmpFtpRsp}" "${tmpResult}"
	DONE=1
    fi
fi

if [ "${DONE}" -eq 0 ]; then
    echo "ERROR: sorry, attempt to fetch chrom.sizes has failed ?" 1>&2
    exit 255
fi

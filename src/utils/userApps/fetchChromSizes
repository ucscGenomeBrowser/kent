#!/bin/sh

# fetchChromSizes - script to grab chrom.sizes from UCSC via either of:
#	mysql, wget or ftp

usage() {
    printf "usage: fetchChromSizes <db> > <db>.chrom.sizes
    used to fetch chrom.sizes information from UCSC for the given <db>
<db> - name of UCSC database, e.g.: hg38, hg18, mm9, etc ...

This script expects to find one of the following commands:
    wget, mysql, or ftp in order to fetch information from UCSC.
Route the output to the file <db>.chrom.sizes as indicated above.
This data is available at the URL:
  http://hgdownload.cse.ucsc.edu/goldenPath/<db>/bigZips/<db>.chrom.sizes

Example:   fetchChromSizes hg38 > hg38.chrom.sizes\n" 1>&2
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
    printf "ERROR: can not find any of the commands: wget mysql or ftp\n" 1>&2
    usage
fi

DONE=0
export DONE

if [ -n "${WGET}" ]; then
    url="http://hgdownload.cse.ucsc.edu/goldenPath/${DB}/bigZips/${DB}.chrom.sizes"
    printf "INFO: trying WGET ${WGET} for database ${DB}\n" 1>&2
    printf "url: %s\n" "${url}" 1>&2
    tmpResult=${DB}.tmp.sizes
    ${WGET} ${url} \
	-O ${tmpResult} 2> /dev/null
    if [ $? -ne 0 -o ! -s "${tmpResult}" ]; then
	printf "WARNING: wget command failed\n" 1>&2
	rm -f "${tmpResult}"
    else
	cat ${tmpResult}
	rm -f "${tmpResult}"
	DONE=1
    fi
fi

if [ "${DONE}" -eq 1 ]; then
    exit 0
fi

if [ -n "${MYSQL}" ]; then
    printf "INFO: trying MySQL ${MYSQL} for database ${DB}\n" 1>&2
    ${MYSQL} -N --user=genome --host=genome-mysql.cse.ucsc.edu -A -e \
	"SELECT chrom,size FROM chromInfo ORDER BY size DESC;" ${DB}
    if [ $? -ne 0 ]; then
	printf "WARNING: mysql command failed\n" 1>&2
    else
	DONE=1
    fi
fi

if [ "${DONE}" -eq 1 ]; then
    exit 0
fi

if [ -n "${FTP}" ]; then
    printf "INFO: trying FTP ${FTP} for database ${DB}\n" 1>&2
    url="ftp://hgdownload.cse.ucsc.edu/goldenPath/${DB}/database/chromInfo.txt.gz"
    printf "url: %s\n" "${url}" 1>&2
    tmpResult="chromInfo.txt.gz"
    tmpFtpRsp=fetchTemp.$$
    printf "user anonymous fetchChromSizes@
cd goldenPath/${DB}/database
get chromInfo.txt.gz ${tmpResult}
bye\n" > ${tmpFtpRsp}
    ${FTP} -n -i hgdownload.cse.ucsc.edu < ${tmpFtpRsp} > /dev/null 2>&1
    if [ $? -ne 0 -o ! -s "${tmpResult}" ]; then
	printf "ERROR: ftp command failed\n" 1>&2
	rm -f "${tmpFtpRsp}" "${tmpResult}"
    else
	cat ${tmpResult} | zcat | cut -f1,2 | sort -k2nr
	rm -f "${tmpFtpRsp}" "${tmpResult}"
	DONE=1
    fi
fi

if [ "${DONE}" -eq 0 ]; then
    printf "ERROR: sorry, attempt to fetch chrom.sizes has failed ?\n" 1>&2
    exit 255
fi

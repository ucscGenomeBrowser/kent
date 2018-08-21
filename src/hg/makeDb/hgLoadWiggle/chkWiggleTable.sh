#!/bin/sh
#
#	chkWiggleTable.sh - verify a wiggle table coordinates are OK
#
# 	source tree location: src/hg/makeDb/hgLoadWiggle/chkWiggleTable.sh

# ident "$Id: chkWiggleTable.sh,v 1.2 2004/09/29 23:10:51 hiram Exp $";

HGWIGGLE=hgWiggle

usage() {
    echo "chkWiggleTable.sh <db> <table>"
    echo -e "\tto check the integrity of wiggle data in the specified table"
    echo -e "\tmust be run on kolossus to handle the largest chromosomes"
}

if [ $# -lt 2 ]; then
	usage
	exit 255
fi

UNAME=`uname -n`

if [ "${UNAME}" != "kolossus" ]; then
    echo "ERROR: must be run on kolossus to handle the largest chromosomes"
    usage
    exit 255
fi

DB=$1
TABLE=$2

echo "Checking Table: ${DB}.${TABLE}"

C=`hgsql -N --host=hgwdev.soe.ucsc.edu --user=hguser --password=hguserstuff \
	-e "describe ${TABLE};" ${DB} | \
	egrep "chromStart|span|dataRange|sumSquares" | wc -l`

if [ "${C}" -ne 4 ]; then
    echo "ERROR: table ${DB}.${TABLE} does not appear to be a wiggle table ?"
    hgsql --host=hgwdev.soe.ucsc.edu --user=hguser --password=hguserstuff \
	-e "describe ${TABLE};" ${DB}
    usage
    exit 255
fi

#	A temporary file to save chromInfo results
CI=/tmp/chkWig_chromInfo.$$

hgsql -N --host=hgwdev.soe.ucsc.edu --user=hguser --password=hguserstuff \
	-e "select * from chromInfo;" ${DB} > "${CI}"

#	A temporary file to construct the bed intersection file

BF=/tmp/chkWig_$$.bed

#	Create two bed lines for each chrom
#	chrN	0	10
#	chrN	<chromEnd-10>	<chromEnd>

awk '
{
	printf "%s\t0\t10\t%s_0\n", $1, $1
	printf "%s\t%d\t%d\t%s_1\n", $1, $2-10, $2, $1
}
' "${CI}" > "${BF}"

WC=`cat "${BF}" | wc -l`

if [ "${WC}" -lt 2 ]; then
    echo "ERROR: having trouble constructing the test bed file ?"
    echo "have the following chromInfo:  (should not be empty)"
    cat "${CI}"
fi

RF=/tmp/chkWig_${DB}.${TABLE}.$$
HGDB_CONF=/tmp/chkWig_hg.conf.$$
export HGDB_CONF
echo "db.host=hgwdev.soe.ucsc.edu" > "${HGDB_CONF}"
echo "db.user=hguser" >> "${HGDB_CONF}"
echo "db.password=hguserstuff" >> "${HGDB_CONF}"
chmod 600 "${HGDB_CONF}"

${HGWIGGLE} -verbose=2 -db="${DB}" \
	-bedFile="${BF}" "${TABLE}" > "${RF}" 2>&1

WARN=`grep WARN "${RF}" | wc -l`

rm -f "${CI}" "${BF}" "${HGDB_CONF}"

if [ "${WARN}" -gt 0 ]; then
    echo "ERROR: there are ${WARN} coordinate problems"
    echo "Results file can be seen in: ${RF}"
else
    echo "Checked Table: ${DB}.${TABLE} - STATUS: OK"
    rm -f "${RF}"
fi

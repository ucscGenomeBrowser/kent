#!/bin/sh
#
#	fixupGbdb.sh - changes references in DB tables columns
#		where file names start with "/gbdb/..." to some
#		other prefix
#
#	$Id: ex.fixupGbdb.sh,v 1.1 2005/03/07 23:24:15 hiram Exp $
#

if [ "$#" -ne 3 ]; then
    echo "usage: fixupGbdb.sh <db.table> <column> </path-prefix>"
    echo -e "\tused to reset /gbdb/ references to the given <path-prefix>"
    echo -e "\tthe column references will be changed from /gbdb/ to <path-prefix>/"
    echo -e "\te.g. ./fixupGbdb.sh cb1.chromInfo fileName /scratch/local/gbdb"
    exit 255;
fi

TBL=$1
COL=$2
PREFIX=$3
export TBL PREFIX

hgsql -e "SELECT ${COL} FROM ${TBL} \
	where ${COL} REGEXP \"^/gbdb/.*\" limit 5;" mysql

hgsql -e "update ${TBL} \
	set ${COL}=REPLACE(${COL}, \"/gbdb\", \"$PREFIX\") \
	where ${COL} REGEXP \"^/gbdb/.*\";" mysql

hgsql -e "SELECT ${COL} FROM ${TBL}; limit 5" mysql

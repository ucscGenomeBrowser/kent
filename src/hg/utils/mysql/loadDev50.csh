#!/bin/csh -fe

# 	$Id: loadDev50.csh,v 1.1 2009/09/04 17:15:23 hiram Exp $

if ( $#argv < 3 || $#argv > 4  ) then
    echo "usage: loadDev50.csh [option] <db> <tableName.sql> <tableName.txt.gz>"
    echo "option:"
    echo "-replace - in the case where <tableName> already exists"
    echo "           to cause a drop and reload of the table."
    echo "e.g.: loadDev50.csh -replace hg19 grp.sql grp.txt.gz"
    exit 255
endif

set REPLACE = ""

if ( $#argv == 3 ) then
    set DB = $1
    set TBL = $2:r
    set TBLSQL = $2
    set TBLDATA = $3
else
    set REPLACE = "-replace"
    if ( "$argv[1]" == "-replace" ) then
	set DB = $2
	set TBL = $3:r
	set TBLSQL = $3
	set TBLDATA = $4
    else if ( "$argv[2]" == "-replace" ) then
	set DB = $1
	set TBL = $3:r
	set TBLSQL = $3
	set TBLDATA = $4
    else if ( "$argv[3]" == "-replace" ) then
	set DB = $1
	set TBL = $2:r
	set TBLSQL = $2
	set TBLDATA = $4
    else if ( "$argv[4]" == "-replace" ) then
	set DB = $1
	set TBL = $2:r
	set TBLSQL = $2
	set TBLDATA = $3
    else
	echo "ERROR: 4 arguments given, not one of which is '-replace'"
	echo "usage: loadDev50.csh [option] <db> <tableName.sql> <tableName.txt.gz>"
	exit 255
    endif
endif

if ( "${REPLACE}" == "-replace" ) then
    echo "NOTE: going to replace ${DB}.${TBL}"
endif

unsetenv HGDB_HOST
unsetenv HGDB_USER
unsetenv HGDB_PASSWORD
set SQL_DST_HOST = hgwdev
set HGSQL = "hgsql -h${SQL_DST_HOST}"

setenv HGDB_HOST ${SQL_DST_HOST}
setenv HGDB_USER hgcat

if ( ! -s ${TBLSQL} ) then
    echo "ERROR: loadDev50: can not read sql file: '${TBLSQL}'" > /dev/stderr
    exit 255
endif
if ( ! -s ${TBLDATA} ) then
    echo "ERROR: loadDev50: can not read data file: '${TBLDATA}'" > /dev/stderr
    exit 255
endif

# Verify we are talking to the correct host during hgsql commands
set TEST_HOST = `${HGSQL} -N -e "show variables;" mysql | egrep "host|pid" | sed -e "s#hostname\t##; s#pid_file\t/var/lib/mysql/##; s#.pid##; s#pid_file\t/var/mysql/5.0/data/##" | sort -u`
if ( "${TEST_HOST}" == "${SQL_DST_HOST}" ) then
    echo "loading table ${DB}.${TBL} into '${TEST_HOST}'"
else
    echo "ERROR: loadDev50: incorrect host '${TEST_HOST}' expecting '${SQL_DST_HOST}'" > /dev/stderr
    exit 255
endif

if ( "${REPLACE}" == "-replace" ) then
    echo "# ${TBL} status before drop:"
    ${HGSQL} -N ${DB} -e 'show table status like "'${TBL}'";' | cat
    echo ${HGSQL} -e "drop table ${TBL};" ${DB}
    ${HGSQL} -e "drop table ${TBL};" ${DB}
else
    set COUNT = `${HGSQL} -N ${DB} -e 'show table status like "'${TBL}'";' | wc -l`
    if ( ${COUNT} != 0 ) then
	echo "ERROR: table ${DB}.${TBL} appears to exist ?"
	echo "use the -replace option to overwrite"
	exit 255
    endif
endif

${HGSQL} ${DB} < ${TBLSQL}
zcat ${TBLDATA} | ${HGSQL} ${DB} \
	-e 'load data local infile "/dev/stdin" into table '"${TBL}"''
echo "# ${TBL} status after load:"
${HGSQL} -N ${DB} -e 'show table status like "'${TBL}'";' | cat

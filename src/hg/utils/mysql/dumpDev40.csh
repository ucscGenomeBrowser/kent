#!/bin/csh -fe

# 	$Id: dumpDev40.csh,v 1.3 2009/09/04 17:48:18 hiram Exp $

if ( $#argv != 2  ) then
    echo "usage: dumpDev40.csh <db> <tableName>"
    echo "      used to fetch archived hgwdev tables from the backup server"
    echo "e.g.: dumpDev40.csh hg18 grp"
    echo "      creates files: grp.sql and grp.txt.gz from hg18"
    echo "Also shows touch command to reset time on database file"
    echo "to maintain Update_time"
    exit 255
endif

unsetenv HGDB_HOST
unsetenv HGDB_USER
unsetenv HGDB_PASSWORD
unsetenv HGDB_CONF
setenv MYSQL_TCP_PORT 3366
setenv MYSQL_HOST backupserv

set SQL_SRC_HOST = backupserv
set DB = $1
set TBL = $2
set HGSQL = "hgsql -P 3366 -h${SQL_SRC_HOST}"

setenv HGDB_HOST ${SQL_SRC_HOST}
setenv HGDB_USER hguser
setenv HGDB_PASSWORD hguserstuff
setenv HGDB_CONF /cluster/home/hiram/.hg.dev40.conf

# Verify we are talking to the correct host during hgsql commands
set TEST_HOST = `${HGSQL} -N -e "show variables;" mysql | egrep "host|pid" | grep -v "^pid_file" | sed -e "s#hostname\t##; s#pid_file\t/var/lib/mysql/##; s# pid_file /data.*##; s#.pid##; s#pid_file\t/var/mysql/5.0/data/##" | sort -u`
if ( "${TEST_HOST}" != "${SQL_SRC_HOST}" ) then
    echo "ERROR: dumpDev40: incorrect host '${TEST_HOST}' expecting '${SQL_SRC_HOST}'" > /dev/stderr
    exit 255
endif

set TSTATUS = `${HGSQL} -N -e 'show table status like "'${TBL}'";' ${DB}`
set ROWS = `echo ${TSTATUS} | awk -F' ' '{print $5}'`
set UPDATE_TIME = `echo ${TSTATUS} | awk -F' ' '{print $14,$15}'`
set TOUCH = `echo ${UPDATE_TIME} | sed -e "s/-//g; s/ //; s/://; s/:/./"`
echo "dumping ${ROWS} rows from table ${DB}.${TBL} from '${TEST_HOST}' SQL server"
echo "touch -c -m -t ${TOUCH} /data/mysql/${DB}/${TBL}.MYD"
hgsqldump -P ${MYSQL_TCP_PORT} -h${SQL_SRC_HOST} -d ${DB} ${TBL} > ${TBL}.sql
${HGSQL} -N -e "select * from ${TBL};" ${DB} | gzip -c > ${TBL}.txt.gz
set ROWS_DUMPED = `zcat ${TBL}.txt.gz | wc -l`
ls -ogrt ${TBL}.sql ${TBL}.txt.gz
if ( ${ROWS_DUMPED} != ${ROWS} ) then
    echo "ERROR: rows dumped ${ROWS_DUMPED} != ${ROWS} rows expected"
    exit 255
endif

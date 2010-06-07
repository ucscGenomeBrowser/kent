#!/bin/csh -fe

# 	$Id: dumpBeta40.csh,v 1.2 2009/08/29 00:30:01 galt Exp $

if ( $#argv != 2  ) then
    echo "usage: dumpBeta40.csh <db> <tableName>"
    echo "e.g.: dumpBeta40.csh hg19 trackDb"
    exit 255
endif

unsetenv HGDB_HOST
unsetenv HGDB_USER
unsetenv HGDB_PASSWORD
unsetenv HGDB_CONF
set SQL_SRC_HOST = mysqlbeta
set DB = $1
set TBL = $2
set HGSQL = "hgsql -h${SQL_SRC_HOST}"

setenv HGDB_HOST ${SQL_SRC_HOST}
setenv HGDB_USER hguser
setenv HGDB_PASSWORD hguserstuff

# Verify we are talking to the correct host during hgsql commands
set TEST_HOST = `${HGSQL} -N -e "show variables;" mysql | egrep "host|pid" | sed -e "s#hostname\t##; s#pid_file\t/var/lib/mysql/##; s#.pid##; s#pid_file\t/var/mysql/5.0/data/##" | sort -u`
if ( "${TEST_HOST}" == "${SQL_SRC_HOST}" ) then
    echo "dumping table ${DB}.${TBL} from '${TEST_HOST}'"
else
    echo "ERROR: dumpBeta40: incorrect host '${TEST_HOST}' expecting '${SQL_SRC_HOST}'" > /dev/stderr
    exit 255
endif

hgsqldump -h${SQL_SRC_HOST} -d ${DB} ${TBL} > ${TBL}.sql
${HGSQL} -N -e "select * from ${TBL};" ${DB} | gzip -c > ${TBL}.txt.gz
ls -ogrt ${TBL}.sql ${TBL}.txt.gz

#!/bin/csh -fe

# 	$Id: dev40ToDev50.csh,v 1.2 2009/09/04 17:17:16 hiram Exp $

if ( $#argv < 2 || $#argv > 3 ) then
    echo "usage: dev40ToDev50.csh [option] <db> <tableName>"
    echo "option:"
    echo "-replace - in the case where <tableName> already exists"
    echo "           to cause a drop and reload of the table."
    echo "e.g.: dev40ToDev50.csh -replace hg18 grp"
    exit 255
endif

set DUMP40 = /cluster/bin/scripts/dumpDev40.csh
set LOAD50 = /cluster/bin/scripts/loadDev50.csh

set REPLACE = ""
set TMP = $$

if ( $#argv == 2 ) then
    set DB = $1
    set TBL = $2
else
    set REPLACE = "-replace"
    if ( "$argv[1]" == "-replace" ) then
	set DB = $2
	set TBL = $3
    else if ( "$argv[2]" == "-replace" ) then
	set DB = $1
	set TBL = $3
    else if ( "$argv[3]" == "-replace" ) then
	set DB = $1
	set TBL = $2
    else
	echo "ERROR: 3 arguments given, not one of which is '-replace'"
	echo "usage: dev40ToDev50.csh [option] <db> <tableName>"
	exit 255
    endif
endif

mkdir /data/tmp/devToDev.${TMP}
cd /data/tmp/devToDev.${TMP}

${DUMP40} ${DB} ${TBL}

${LOAD50} ${REPLACE} ${DB} ${TBL}.sql ${TBL}.txt.gz

rm -f ${TBL}.sql ${TBL}.txt.gz
cd /tmp
rmdir /data/tmp/devToDev.${TMP}

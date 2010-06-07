#!/bin/csh -fe

# 	$Id: beta40ToBeta50.csh,v 1.1 2009/08/21 18:08:53 hiram Exp $

if ( $#argv < 2 || $#argv > 3 ) then
    echo "usage: beta40ToBeta50.csh [option] <db> <tableName>"
    echo "option:"
    echo "-replace - in the case where <tableName> already exists"
    echo "           to cause a drop and reload of the table."
    echo "e.g.: beta40ToBeta50.csh -replace hg19 trackDb"
    exit 255
endif

set DUMP40 = /cluster/bin/scripts/dumpBeta40.csh
set LOAD50 = /cluster/bin/scripts/loadBeta50.csh

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
	echo "usage: beta40ToBeta50.csh [option] <db> <tableName>"
	exit 255
    endif
endif

mkdir /data/tmp/betaToBeta.${TMP}
cd /data/tmp/betaToBeta.${TMP}

${DUMP40} ${DB} ${TBL}

${LOAD50} ${REPLACE} ${DB} ${TBL}.sql ${TBL}.txt.gz

rm -f ${TBL}.sql ${TBL}.txt.gz
cd /tmp
rmdir /data/tmp/betaToBeta.${TMP}

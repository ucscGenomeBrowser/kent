#!/bin/sh
#
#	activeDbList.sh - fetch list of active databases
#
#	$Id: activeDbList.sh,v 1.1 2010/03/18 23:18:23 hiram Exp $

# fetch list of active databases from public MySQL server

if [ "X${1}Y" = "XgoY" ]; then
mysql -N -A -hgenome-mysql.soe.ucsc.edu -ugenomep -ppassword \
	-e "select name from dbDb where active=1;" hgcentral \
	| sort
else
    echo "activeDbList.sh - fetch list of active databases from public"
    echo "  UCSC Genome MySQL server.  Use an argument: go"
    echo "  to get this to run.  Send the result to a file.  e.g.:"
    echo "./activeDbList.sh go > active.db.list.txt"
fi

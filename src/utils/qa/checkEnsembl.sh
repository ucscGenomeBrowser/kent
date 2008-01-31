#!/bin/sh

#	Do not modify this script, modify the source tree copy:
#	src/utils/qa/checkEnsembl.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

#	$Id: checkEnsembl.sh,v 1.1 2008/01/31 23:59:28 hiram Exp $

# set EMAIL here for notification list
EMAIL="xyz@ucsc.edu"
# set DEBUG_EMAIL here for notification of potential errors in the process
DEBUG_EMAIL="hiram"

#	cron jobs need to ensure this is true
umask 002

#	this is where we are going to work
cd /cluster/data/ensembl/checkNew

#	create ftp response script for the ftp connection session
rm -f ftp.ensembl.org.rsp
echo "user anonymous ${DEBUG_EMAIL}
cd pub
ls
bye" > ftp.ensembl.org.rsp

#	reorganize results files
rm -f prev.release.list
rm -f ls.check
cp -p release.list prev.release.list
rm -f release.list

#	connect and list a directory, result to file: ls.check
ftp -n -v -i ftp.ensembl.org < ftp.ensembl.org.rsp > ls.check
#	fetch the release directory names from the ls.check result file
grep " release-[0-9][0-9]*$" ls.check | awk '{print $NF}' | sort > release.list

#	verify we are getting a proper list
WC=`cat release.list | wc -l`
if [ "${WC}" -lt 5 ]; then
    echo "potential error in Ensembl release watch,
check release.list in /cluster/data/ensembl/checkNew" \
	| mail -s "ERROR: Ensembl watch" ${DEBUG_EMAIL}
    exit 255
fi

#	see if anything is changing, if so, email notify
S0=`sum -r prev.release.list`
S1=`sum -r release.list`
if [ "${S0}" != "${S1}" ]; then
    echo -e "New Ensembl release noted at:\n" \
"ftp://ftp.ensembl.org/pub/"`comm -13 prev.release.list release.list`"/" \
    | mail -s "Ensembl release watch" ${EMAIL}
fi

#!/bin/sh

#	Do not modify this script, modify the source tree copy:
#	src/utils/decipher/checkDecipher.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

# set EMAIL here for notification list
EMAIL="fanhsu@soe.ucsc.edu"
# set DEBUG_EMAIL here for notification of potential errors in the process
DEBUG_EMAIL="fanhsu@soe.ucsc.edu"

#	cron jobs need to ensure this is true
umask 002

WORKDIR="/hive/groups/gsid/medical/fan/decipher/auto"
export WORKDIR

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in DECIPHER release watch, Can not find the directory:
    ${WORKDIR}" \
	| mail -s "ERROR: DECIPHER watch" ${DEBUG_EMAIL}
    exit 255
fi

cd "${WORKDIR}"

ftppass=`cat ftp.pwd`
gpgpass=`cat gpg.pwd`

#	create ftp response script for the ftp connection session
rm -f ftp.decipher.rsp
echo "user ftp-decipher-dda ${ftppass}
cd pub
ls
bye" > ftp.decipher.rsp

#	reorganize results files
rm -f prev.release.list
rm -f ls.check
cp -p release.list prev.release.list
rm -f release.list

#	connect and list a directory, result to file: ls.check
ftp -n -v -i ftp.sanger.ac.uk  < ftp.decipher.rsp > ls.check

#	fetch the release directory names from the ls.check result file
grep "decipher-" ls.check | sort > release.list
chmod o+w release.list

#	verify we are getting a proper list
WC=`cat release.list | wc -l`
if [ "${WC}" -lt 1 ]; then
    echo "potential error in DECIPHER release watch,
check release.list in ${WORKDIR}" \
	| mail -s "ERROR: DECIPHER watch" ${DEBUG_EMAIL}
    exit 255
fi

#	see if anything is changing, if so, email notify, download, and build

diff prev.release.list release.list  >release.diff
WC=`cat release.diff | wc -l`
if [ "${WC}" -gt 1 ]; then
    echo -e "New DECIPHER update noted at:\n" \
"ftp://ftp.sanger.ac.uk/pub/\n"`comm -13 prev.release.list release.list`"/" \
    | mail -s "DECIPHER update watch" ${EMAIL}

FN=`cat release.diff |grep decipher-|sed -e 's/decipher-/\tdecipher-/'|cut -f 2`

today=`date +%F`
mkdir -p $today
cd $today

echo "${FN}" >j.fn.gpg
cat j.fn.gpg |sed -e 's/.gpg//' >j.fn.txt

# prepare ftp download response file
rm -f ftp.decipher.rsp
echo "user ftp-decipher-dda ${ftppass}
cd pub
get "${FN}"
bye" > ftp.decipher.rsp

# download the new data file
ftp -n -v -i ftp.sanger.ac.uk  < ftp.decipher.rsp > ftp.log

#rm -f `cat j.fn.txt`

# unpack the gpg encrypted file
gpg --passphrase "${gpgpass}"  "${FN}"

# build the new DECIPHER track tables
../buildDecipher `cat j.fn.txt`

rm j.fn.txt
rm j.fn.gpg
fi


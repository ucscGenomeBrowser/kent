#!/bin/sh -ex

#	Do not modify this script, modify the source tree copy:
#	src/utils/decipher/checkDecipher.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

#	cron jobs need to ensure this is true
umask 002

WORKDIR="/hive/data/outside/otto/decipher"
export WORKDIR

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in DECIPHER release watch, Can not find the directory:
    ${WORKDIR}" 
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
grep "decipher-hg19" ls.check | sort > release.list
chmod o+w release.list

#	verify we are getting a proper list
WC=`cat release.list | wc -l`
if [ "${WC}" -lt 1 ]; then
    echo "potential error in DECIPHER release watch,
check release.list in ${WORKDIR}" 
    exit 255
fi

#	see if anything is changing, if so, email notify, download, and build

diff prev.release.list release.list  >release.diff || true
WC=`cat release.diff | wc -l`
if [ "${WC}" -gt 1 ]; then
    echo -e "New DECIPHER update noted at:\n" \
"ftp://ftp.sanger.ac.uk/pub/\n"`comm -13 prev.release.list release.list`"/" 


    FN=`cat release.diff |grep decipher-| grep txt | sed -e 's/decipher-/\tdecipher-/'|cut -f 2 | tail -1`


    today=`date +%F`
    mkdir -p $today
    cd $today
    cp -p ../*.diff .
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
    ../validateDecipher.sh hg19

    # now install
    for i in `cat ../decipher.tables`
    do 
	n=$i"New"
	o=$i"Old"
	hgsqlSwapTables hg19 $n $i $o -dropTable3
    done

    echo "DECIPHER Installed `date`" 

    rm j.fn.txt
    rm j.fn.gpg
fi


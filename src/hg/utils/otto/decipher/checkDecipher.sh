#!/bin/sh -ex

#	Do not modify this script, modify the source tree copy:
#	src/utils/decipher/checkDecipher.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

#	cron jobs need to ensure this is true
#       current login requires the user be braney
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

#ftppass=`cat ftp.pwd`
gpgpass=`cat gpg.pwd`

perl login.perl

if test decipher-* -nt lastUpdate
then
    today=`date +%F`
    mkdir -p $today

    FN=decipher-*.gpg
    cp -p $FN $today

    cd $today

    # unpack the gpg encrypted file
    gpg --batch --passphrase "${gpgpass}"  ${FN}

    # build the new DECIPHER track tables
    ../buildDecipher `basename $FN .gpg`
    ../validateDecipher.sh hg19

    # now install
    for i in `cat ../decipher.tables`
    do 
	n=$i"New"
	o=$i"Old"
	hgsqlSwapTables hg19 $n $i $o -dropTable3
    done

    echo "DECIPHER Installed `date`" 

    cp -p $FN ../lastUpdate
fi

rm decipher-*

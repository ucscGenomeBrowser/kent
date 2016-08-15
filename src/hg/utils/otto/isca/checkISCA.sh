#!/bin/bash

#	Do not modify this script, modify the source tree copy:
#	src/utils/isca/checkISCA.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

set -eEu -o pipefail

cleanUpOnError () {
    echo "Restoring old release list after failed run; new ls.check remains for debugging"
    cd "${WORKDIR}"
    rm -f release.list
    mv prev.release.list release.list
    if [ -e old.release.list ]
    then
        mv old.release.list prev.release.list
    fi
} 

trap cleanUpOnError ERR
trap "cleanUpOnError; exit 1" SIGINT SIGTERM

#	cron jobs need to ensure this is true
umask 002

WORKDIR="/hive/data/outside/otto/isca"
export WORKDIR
#	this is where we are going to work 
if [ ! -d "${WORKDIR}" ]; then echo "ERROR in ISCA release watch, Can not find the directory:
    ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"

rm -f ftp.isca.rsp
echo "user anonymous otto@soe.ucsc.edu
cd /pub/dbVar/data/Homo_sapiens/by_study/nstd45_ClinGen_Curated_Dosage_Sensitivity_Map
ls
cd /pub/dbVar/data/Homo_sapiens/by_study/nstd101_ClinGen_Kaminsky_et_al_2011
ls
cd /pub/dbVar/data/Homo_sapiens/by_study/nstd37_ClinGen_Laboratory-Submitted
ls
bye" > ftp.isca.rsp

#	reorganize results files
rm -f ls.check
rm -f old.release.list
if [ -e prev.release.list ]
then
    mv prev.release.list old.release.list
fi
cp -p release.list prev.release.list
rm -f release.list

#	connect and list a directory, result to file: ls.check
ftp -n -v -i ftp.ncbi.nlm.nih.gov  < ftp.isca.rsp > ls.check

#	fetch the release directory names from the ls.check result file
grep "gvf" ls.check | sort > release.list
chmod o+w release.list

#	verify we are getting a proper list
WC=`cat release.list | wc -l`
if [ "${WC}" -lt 1 ]; then
    echo "potential error in ISCA release watch,
no gvf files found. Check ls.check in ${WORKDIR}" 
    cleanUpOnError
    exit 255
fi

#	see if anything is changing, if so, email notify, download, and build
diff prev.release.list release.list  >release.diff || true
WC=`cat release.diff | wc -l`
if [ "${WC}" -gt 1 ]; then
    echo -e "New ISCO update noted at:\n" \
"ftp://ftp.ncbi.nlm.nih.gov/pub/\n"`comm -13 prev.release.list release.list`"/" 

    today=`date +%F`
    mkdir -p $today
    cd $today
    mkdir hg18 hg19 hg38
#    csh ../buildISCA.csh hg18
    csh ../buildISCA.csh hg19
    csh ../buildISCA.csh hg38
#    ../validateISCA.sh hg18
    ../validateISCA.sh hg19
    ../validateISCA.sh hg38

    # now install
    for i in `cat ../isca.tables`
    do 
	n=$i"New"
	o=$i"Old"
#	hgsqlSwapTables hg18 $n $i $o -dropTable3
	hgsqlSwapTables hg19 $n $i $o -dropTable3
	hgsqlSwapTables hg38 $n $i $o -dropTable3
    done

    rm -f ../old.release.list
    echo "ISCA Installed `date`" 
fi


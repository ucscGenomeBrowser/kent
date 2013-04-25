#!/bin/sh -ex

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/isca/checkISCA.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

#	cron jobs need to ensure this is true
umask 002

WORKDIR="/hive/data/outside/otto/isca"
export WORKDIR

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in ISCA release watch, Can not find the directory:
    ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"

rm -f ftp.isca.rsp
echo "user anonymous otto@soe.ucsc.edu
cd pub/dbVar/data/Homo_sapiens/by_study/nstd37_ISCA
ls
bye" > ftp.isca.rsp

#	reorganize results files
rm -f prev.release.list
rm -f ls.check
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
check release.list in ${WORKDIR}" 
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
    ../buildISCA.csh
    ../validateISCA.sh hg19

    # now install
    for i in `cat ../isca.tables`
    do 
	n=$i"New"
	o=$i"Old"
	hgsqlSwapTables hg19 $n $i $o -dropTable3
    done

    echo "ISCA Installed `date`" 
fi


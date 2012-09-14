#!/bin/sh -e

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/omim/checkOmim.sh

# set EMAIL here for notification list
#EMAIL="braney@soe.ucsc.edu"
# set DEBUG_EMAIL here for notification of potential errors in the process
#DEBUG_EMAIL="braney@soe.ucsc.edu"

#	cron jobs need to ensure this is true
umask 002

WORKDIR=$1
export WORKDIR
export PATH=$WORKDIR":$PATH"

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in OMIM release watch, Can not find the directory:     ${WORKDIR}" 
#    echo "ERROR in OMIM release watch, Can not find the directory:
#    ${WORKDIR}" \
#	| mail -s "ERROR: OMIM watch" ${DEBUG_EMAIL}
    exit 255
fi

cd "${WORKDIR}"

ftppass=`cat ftp.pwd`
ftppass2=`cat ftp2.pwd`

#	create ftp response script for the ftp connection session
rm -f ftp.omim.rsp
echo "user anonymous ${ftppass}
cd OMIM
ls
bye" > ftp.omim.rsp


#	reorganize results files
rm -f prev.release.list
rm -f ls.check
cp -p release.list prev.release.list
rm -f release.list

#	connect and list a directory, result to file: ls.check
ftp -n -v -i grcf.jhmi.edu  < ftp.omim.rsp > ls.check

#	fetch the release directory names from the ls.check result file
grep "genemap" ls.check |grep -v key|sort > release.list
chmod o+w release.list

#	verify we are getting a proper list
WC=`cat release.list | wc -l`
if [ "${WC}" -lt 1 ]; then
    echo "potential error in OMIM release watch,
check release.list in ${WORKDIR}" 
#	| mail -s "ERROR: OMIM watch" ${DEBUG_EMAIL}
    exit 255
fi

#	see if anything is changing, if so, email notify, download, and build

diff prev.release.list release.list  >release.diff || true
WC=`cat release.diff | wc -l`
if [ "${WC}" -gt 1 ]; then
    echo -e "New OMIM update noted at:\n" \
"ftp://grcf.jhmi.edu/\n"`comm -13 prev.release.list release.list`"/" 
#    | mail -s "OMIM update watch" ${EMAIL}

FN=`cat release.diff |grep omim-|sed -e 's/omim-/\tomim-/'|cut -f 2`

today=`date +%F`
mkdir -p $today
cd $today

# prepare ftp2 download response file
echo doing ftp2 ...

#	create ftp response script for the ftp2 connection session
rm -f ftp2.omim.rsp
echo "user omimftp3 ${ftppass2}
cd OMIM
get mimAV.txt
get geneMap2.txt
bye" > ftp2.omim.rsp

# download the new mimAv.txt data file
ftp -n -v -i grcf.jhmi.edu  < ftp2.omim.rsp > ftp2.log

# prepare ftp download response file
echo doing ftp ...

rm -f ftp.omim.rsp
echo "user anonymous ${ftppass}
cd OMIM
binary
get genemap
get mim2gene.txt
get morbidmap
bye" > ftp.omim.rsp

# download the new data file
ftp -n -v -i grcf.jhmi.edu  < ftp.omim.rsp > ftp.log

# build the new OMIM track tables for hg18
rm -rf hg18
mkdir -p hg18
cd hg18

ln -s ../genemap ./genemap
ln -s ../mimAV.txt ./mimAV.txt
ln -s ../mim2gene.txt ./mim2gene.txt
ln -s ../../parseGeneMap.pl ./parseGeneMap.pl

../../buildOmimTracks.csh hg18
../../validateOmim.sh hg18
cd ..

# build the new OMIM track tables for hg19
rm -rf hg19
mkdir -p hg19
cd hg19

ln -s ../genemap ./genemap
ln -s ../mimAV.txt ./mimAV.txt
ln -s ../mim2gene.txt ./mim2gene.txt
ln -s ../../parseGeneMap.pl ./parseGeneMap.pl

../../buildOmimTracks.csh hg19
../../validateOmim.sh hg19
cd ..

# now install for both hg18 and hg19
for i in `cat ../omim.tables`
do 
    n=$i"New"
    o=$i"Old"
    hgsqlSwapTables hg18 $n $i $o -dropTable3
    hgsqlSwapTables hg19 $n $i $o -dropTable3
done

echo "Omim Installed `date`" 

else
    echo "No update `date` "
fi


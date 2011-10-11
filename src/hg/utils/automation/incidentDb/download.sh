#!/bin/sh

# these settings will make this script exit on any error at any time
set -beEu -o pipefail

export scriptName="download"
# empty previousStep means no previous step to verify as done
export previousStep=""
export TOP="/hive/data/outside/ncbi/incidentDb/testAuto"

. ${TOP}/commonHeader.inc

#############################################################################
# this nonsense is required because the only download process we have
# for NCBI files is FTP via wget.  It doesn't have a --delete option
# that would normally be used in an rsync.
#
# compare this directory contents with .listing and remove files that
#	may have disappeared
cleanDeleted() {
ls *.xml | sort > thisDir.list.txt
# to avoid the grep from failing, add this useless line, remove it with sed
${ECHO} "notThisOne.xml" >> thisDir.list.txt
grep "^\-" .listing | awk '{print $NF}' | grep ".xml" \
	| sed -e 's/\r//g' | sort > theirDir.list.txt
LC=`comm -23 thisDir.list.txt theirDir.list.txt | grep xml | sed -e "/notThisOne.xml/d" | wc -l`
if [ "${LC}" -gt 0 ]; then
    comm -23 thisDir.list.txt theirDir.list.txt | grep xml | sed -e "/notThisOne.xml/d" | while read F
    do
	${ECHO} "rm -f \"${F}\""
	rm -f "${F}"
    done
else
    ${ECHO} "# no files to delete from: "`pwd`
fi
}

#############################################################################
runOne() {
export name=$1
cd "${TOP}"
mkdir -p ${name}
wget --timestamping --level=2 -r --cut-dirs=2 --no-host-directories \
	--no-remove-listing ftp://ftp.ncbi.nih.gov/pub/grc/${name}
cd ${name}
cleanDeleted
cd ..
}

#############################################################################
############  Get to work here
for N in human mouse zebrafish
do
    runOne $N
done

cd "${workDir}"
${ECHO} "# download complete "`date '+%Y-%m-%d %H:%M:%S'` > "${signalDone}"
rm -f "${signalRunning}"
exit 0

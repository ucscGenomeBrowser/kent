
#!/bin/sh -e

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/omim/checkOmimUpload.sh

# set EMAIL here for notification list
#EMAIL="braney@soe.ucsc.edu"
# set DEBUG_EMAIL here for notification of potential errors in the process
#DEBUG_EMAIL="braney@soe.ucsc.edu"

#	cron jobs need to ensure this is true
umask 002

WORKDIR=$1
export WORKDIR
export PATH=$WORKDIR":$PATH"
export UPLOADDIR="/usr/local/apache/htdocs/omimUpload"

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in OMIM release watch, Can not find the directory:     ${WORKDIR}" 
#    echo "ERROR in OMIM release watch, Can not find the directory:
#    ${WORKDIR}" \
#	| mail -s "ERROR: OMIM watch" ${DEBUG_EMAIL}
    exit 255
fi

cd "${WORKDIR}"/upload

rm -f omimGene2.date
touch omimGene2.date
hgsqlTableDate hg19 omimGene2 omimGene2.date
if test \! omimGene2.date -nt upload.omim2.date
then
    echo "No new table."
    exit 0;
fi
echo doing upload

# assumes hg18 and hg19 have the same tables
hgsql -h hgwbeta -N -e "SHOW TABLES LIKE 'omim%' " hg19 > omimTables
rm -f omimTableDump.tar omimTableDump.tar.gz

for db in hg18 hg19 hg38
do
    for table in `cat omimTables`
    do
#	echo $db.$table.tab >> pushList
	hgsql -N -e "SELECT * FROM $table" $db > $db.$table.tab
	tar -rf omimTableDump.tar $db.$table.tab
  rm $db.$table.tab
    done
done

if [ ! -e omimTableDump.tar ]
then
  echo "Process failed"
  exit 1
fi

gzip omimTableDump.tar
cp -p omimTableDump.tar.gz "$UPLOADDIR/omimTableDump.tgz"

mv omimGene2.date upload.omim2.date

echo "Process successful"
exit 0

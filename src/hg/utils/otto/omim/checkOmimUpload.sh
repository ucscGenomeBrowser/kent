
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

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in OMIM release watch, Can not find the directory:     ${WORKDIR}" 
#    echo "ERROR in OMIM release watch, Can not find the directory:
#    ${WORKDIR}" \
#	| mail -s "ERROR: OMIM watch" ${DEBUG_EMAIL}
    exit 255
fi

cd "${WORKDIR}"/upload

rm omimGene2.date
touch omimGene2.date
hgsqlTableDate hg19 omimGene2 omimGene2.date
if test \! omimGene2.date -nt upload.omim2.date
then
    echo "No new table."
    exit 0;
fi
echo doing upload

# assumes hg18 and hg19 have the same tables
hgsql -h mysqlbeta -N -e "SHOW TABLES LIKE 'omim%' " hg19 > omimTables
rm -f pushList

for db in hg18 hg19 hg38
do
    for table in `cat omimTables`
    do
	echo $db.$table.tab >> pushList
	hgsql -N -e "SELECT * FROM $table" $db > $db.$table.tab
    done
done

# create script to scp the files
rm -f scp.script
for i in `cat pushList`
do
    echo scp -q -i omim.ftp.key  $i  omimftp@ftp.omim.org:/home/omimftp1/OMIM/$i
done >> scp.script

if sh -e scp.script
then
    mv omimGene2.date upload.omim2.date
    echo "Process successful"
else
    echo "Process failed"
    exit 1
fi

exit 0

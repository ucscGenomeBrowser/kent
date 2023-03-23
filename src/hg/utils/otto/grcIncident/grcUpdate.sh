#!/bin/bash

# exit on any failure
set -beEu -o pipefail

export TOP="/hive/data/outside/otto/grcIncidentDb"

export bbiInfo="/cluster/bin/x86_64/bigBedInfo"
export failMail="hiram@soe.ucsc.edu,otto-group@ucsc.edu"

if [ $# -ne 4 ]; then
  echo "usage: grcUpdate.sh <workDir> <db> <issueName> <ftpPath>" 1>&2
  exit 255
fi

export debug=0
export workDir="$1"
export db="$2"
# This ^ trick upper cases the first letter of the string in variable db
export Db=${db^}
export GRC_issue="$3"
export ftpPath="$4/${GRC_issue}.gff3"

cd "${TOP}/${workDir}"

if [ $debug -ne 0 ]; then
  rm -f updateRunning.pid
fi

if [ -s updateRunning.pid ]; then
  printf "To: $failMail\nFrom: $failMail\nSubject: ERROR: GRC Incident update $Db\n\nERROR: ${db} GRC incident update, previous update did not finish\n" | /usr/sbin/sendmail -t -oi

#  echo "ERROR: ${db} GRC incident update, previous update did not finish" \
#        | /bin/mail -s "ERROR: GRC Incident update $Db" ${failMail}
#            > /dev/null 2> /dev/null
  exit 245
fi

echo $$ > updateRunning.pid

export YM=`date "+%Y/%m"`
mkdir -p ${YM}
export DS=`date "+%F"`

if [ $debug -ne 0 ]; then
  printf "# %s\n" "https://ftp.ncbi.nlm.nih.gov/pub/grc/${ftpPath}" 1>&2
  printf "# into ${YM}/${GRC_issue}.${DS}.gff\n" 1>&2
fi

wget --timestamping \
 https://ftp.ncbi.nlm.nih.gov/pub/grc/${ftpPath} \
   -O ${YM}/${GRC_issue}.${DS}.gff > /dev/null 2>&1
gzip -f ${YM}/${GRC_issue}.${DS}.gff
if [ $debug -ne 0 ]; then
  ls -og ${YM}/${GRC_issue}.${DS}.gff.gz 1>&2
fi

if [ $debug -ne 0 ]; then
  printf "# fetched into ${workDir}/${YM}/${GRC_issue}.${DS}.gff\n" 1>&2
fi

if [ "${db}" = "mm9" ]; then
/hive/data/outside/otto/grcIncidentDb/parseGff.pl \
  ${YM}/${GRC_issue}.${DS}.gff.gz | sort \
     |  join -t$'\t' validContigs - > ${YM}/${db}.${DS}.contigs.bed5
  /cluster/bin/x86_64/liftUp -type=.bed ${YM}/${db}.${DS}.bed5 refSeq.lift error ${YM}/${db}.${DS}.contigs.bed5
  gzip -f ${YM}/${db}.${DS}.contigs.bed5
else
  /hive/data/outside/grc/incidentDb/parseGff.pl \
    ${YM}/${GRC_issue}.${DS}.gff.gz | sort \
     | join -t$'\t' refSeq.chromNames.tab - \
      | cut -f2- | sort -k1,1 -k2,2n > ${YM}/${db}.${DS}.bed5
fi

/cluster/bin/x86_64/bedToBigBed -type=bed4+1 \
  -as=$HOME/kent/src/hg/lib/grcIncidentDb.as \
      ${YM}/${db}.${DS}.bed5 /hive/data/genomes/${db}/chrom.sizes \
         ${YM}/${DS}.${Db}.grcIncidentDb.bb > /dev/null 2>&1

gzip -f ${YM}/${db}.${DS}.bed5
if [ $debug -ne 0 ]; then
  printf "# parsed into ${YM}/${db}.${DS}.bed5.gz\n" 1>&2
  ls -og ${YM}/${db}.${DS}.bed5.gz 1>&2
fi

export newSum=`md5sum -b ${YM}/${DS}.${Db}.grcIncidentDb.bb | awk '{print $1}'`
export oldSum=0
if [ -s ${Db}.grcIncidentDb.bb ]; then
  oldSum=`md5sum -b ${Db}.grcIncidentDb.bb | awk '{print $1}'`
fi

if [ $debug -ne 0 ]; then
  printf "# newSum $newSum ${workDir}/${YM}/${DS}.${Db}.grcIncidentDb.bb\n" 1>&2
  ls -og ${YM}/${DS}.${Db}.grcIncidentDb.bb 1>&2
  printf "# oldSum $oldSum ${workDir}/${Db}.grcIncidentDb.bb\n" 1>&2
  ls -og ${Db}.grcIncidentDb.bb 1>&2
fi

if [ "${oldSum}" = "${newSum}" ]; then
#    echo "${db} GRC update no change from previous ${DS}" \
#         | mail -s "ALERT: GRC Incident update $Db" ${failMail} \
#             > /dev/null 2> /dev/null
   rm -f ${YM}/${DS}.${Db}.grcIncidentDb.bb
   rm -f ${YM}/${db}.${DS}.bed5.gz
   rm -f ${YM}/${GRC_issue}.${DS}.gff.gz
   rm -f ${YM}/${db}.${DS}.contigs.bed5.gz
else
   rm -f ${Db}.grcIncidentDb.bb
   cp -p ${YM}/${DS}.${Db}.grcIncidentDb.bb ${Db}.grcIncidentDb.bb
   printf "To: $failMail\nFrom: $failMail\nSubject: ALERT: GRC Incident update $Db\n\n=== Updated $Db ===\n%s\n" \
      "`${bbiInfo} -udcDir=./udcCache ${Db}.grcIncidentDb.bb`" \
	| /usr/sbin/sendmail -t -oi
   rm -fr ./udcCache
# no longer necessary
   # /cluster/bin/scripts/gwUploadFile ${Db}.grcIncidentDb.bb ${Db}.grcIncidentDb.bb > /dev/null 2>&1
   url=`/cluster/bin/x86_64/hgsql -N -e "select * from grcIncidentDb;" $db`
   rm -fr ./udcCache
   mkdir ./udcCache
   ${bbiInfo} -udcDir=./udcCache "${url}" 2>&1 \
        | mail -s "ALERT: GRC Incident update $Db" ${failMail} \
            > /dev/null 2> /dev/null
   rm -fr ./udcCache
   zcat ${YM}/${db}.${DS}.bed5.gz | cut -f4 | sort -u | awk '{printf "%s\t%s\n", $1, $1}' > ${Db}.nameIndex.txt
   /cluster/bin/x86_64/ixIxx ${Db}.nameIndex.txt ${Db}.grcIncidentDb.ix ${Db}.grcIncidentDb.ixx
fi
rm -f updateRunning.pid
printf "SUCCESS\n"
exit 0

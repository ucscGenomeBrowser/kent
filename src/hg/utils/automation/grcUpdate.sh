#!/bin/bash

# exit on any failure
set -beEu -o pipefail

TOP="/hive/data/outside/grc/incidentDb"

export ECHO="/bin/echo -e"
export bbiInfo="/cluster/bin/x86_64/bigBedInfo"
export failMail="hiram@soe.ucsc.edu"

if [ $# -ne 4 ]; then
  echo "usage: grcUpdate.sh <workDir> <db> <issueName> <ftpPath>" 1>&2
  exit 255
fi

export workDir="$1"
export db="$2"
# This ^ trick upper cases the first letter of the string in variable db
export Db=${db^}
export GRC_issue="$3"
export ftpPath="$4/${GRC_issue}.gff3"

cd "${TOP}/${workDir}"

if [ -s updateRunning.pid ]; then
  echo "ERROR: ${db} GRC incident update, previous update did not finish" \
        | mail -s "ERROR: GRC Incident update $Db" ${failMail} \
            > /dev/null 2> /dev/null
  exit 255
fi

echo $$ > updateRunning.pid

export YM=`date "+%Y/%m"`
mkdir -p ${YM}
export DS=`date "+%F"`

wget --timestamping \
 ftp://ftp.ncbi.nlm.nih.gov/pub/grc/${ftpPath} \
   -O ${YM}/${GRC_issue}.${DS}.gff > /dev/null 2>&1
gzip -f ${YM}/${GRC_issue}.${DS}.gff

if [ "${db}" = "mm9" ]; then
/hive/data/outside/grc/incidentDb/parseGff.pl ${YM}/${GRC_issue}.${DS}.gff.gz \
  | sort \
     |  join -t"	" validContigs - > ${YM}/${db}.${DS}.contigs.bed5
  /cluster/bin/x86_64/liftUp -type=.bed ${YM}/${db}.${DS}.bed5 refSeq.lift error ${YM}/${db}.${DS}.contigs.bed5
  gzip -f ${YM}/${db}.${DS}.contigs.bed5
else
  /hive/data/outside/grc/incidentDb/parseGff.pl \
    ${YM}/${GRC_issue}.${DS}.gff.gz | sort \
     | join -t"	" refSeq.chromNames.tab - \
      | cut -f2- | sort -k1,1 -k2,2n > ${YM}/${db}.${DS}.bed5
fi

/cluster/bin/x86_64/bedToBigBed -type=bed4+1 \
  -as=$HOME/kent/src/hg/lib/grcIncidentDb.as \
      ${YM}/${db}.${DS}.bed5 /hive/data/genomes/${db}/chrom.sizes \
         ${YM}/${DS}.${Db}.grcIncidentDb.bb > /dev/null 2>&1

gzip -f ${YM}/${db}.${DS}.bed5

export newSum=`md5sum -b ${YM}/${DS}.${Db}.grcIncidentDb.bb | awk '{print $1}'`
export oldSum=0
if [ -s ${Db}.grcIncidentDb.bb ]; then
  oldSum=`md5sum -b ${Db}.grcIncidentDb.bb | awk '{print $1}'`
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
   /cluster/bin/scripts/gwUploadFile ${Db}.grcIncidentDb.bb ${Db}.grcIncidentDb.bb > /dev/null 2>&1
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
${ECHO} SUCCESS
exit 0

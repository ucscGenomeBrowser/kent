#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  echo "usage: ./quickDbBuild.sh <db>" 1>&2
  echo "where <db> is the UCSC database name." 1>&2
  echo "Will verify that the <db> does not yet exist before continuing." 1>&2
  echo "and expects to find chrom.sizes, <db>.unmasked.2bit and <db>.agp" 1>&2
  echo "existing in /hive/data/genomes/<db>/" 1>&2
  exit 255
fi

export DB=$1

export tableCount=`(hgsql -N -e 'show table status;' ${DB} 2> /dev/null || /bin/true) | wc -l`

if [ ${tableCount} -gt 0 ]; then
  echo "ERROR: found $tableCount tables in database '${DB}', already exists ?" 1>&2
  hgsql -e 'show tables;' ${DB} 1>&2
  exit 255
fi

export dataDir="/hive/data/genomes/${DB}"
export fileCount=0
for F in chrom.sizes ${DB}.unmasked.2bit ${DB}.agp
do
   if [ -s ${dataDir}/${F} ]; then
      let "fileCount = fileCount+1"
   else
      echo "ERROR: can not find required file: ${dataDir}/${F}" 1>&2
   fi
done

if [ "$fileCount" -ne 3 ]; then
   echo "ERROR: cannot find required file(s) as indicated" 1>&2
   exit 255
fi

mkdir -p /hive/data/genomes/${DB}/bed/chromInfo
cd /hive/data/genomes/${DB}
awk '{print $1 "\t" $2 "\t/gbdb/'$DB/$DB'.2bit";}' chrom.sizes \
  > bed/chromInfo/chromInfo.tab

hgsql -e "create database $DB;" test

hgGoldGapGl -noGl ${DB} ${DB}.agp

cut -f1 bed/chromInfo/chromInfo.tab | awk '{print length($0)}' \
   | sort -nr > bed/chromInfo/t.chrSize
export chrSize=`head -1 bed/chromInfo/t.chrSize`
sed -e "s/chrom(16)/chrom($chrSize)/" \
   ${HOME}/kent/src/hg/lib/chromInfo.sql > bed/chromInfo/chromInfo.sql
rm -f bed/chromInfo/t.chrSize
hgLoadSqlTab ${DB} chromInfo bed/chromInfo/chromInfo.sql \
   bed/chromInfo/chromInfo.tab

hgsql $DB < $HOME/kent/src/hg/lib/grp.sql
mkdir -p /gbdb/${DB}

ln -s /hive/data/genomes/${DB}/${DB}.unmasked.2bit /gbdb/${DB}/${DB}.2bit
mkdir $HOME/kent/src/hg/makeDb/trackDb/assemblyHubs/${DB}

cd $HOME/kent/src/hg/makeDb/trackDb

make EXTRA="-strict" DBS=$DB
make GIT=echo EXTRA="-strict" DBS=$DB alpha

rm -f /gbdb/$DB/trackDb.ix /gbdb/$DB/trackDb.ixx

exit $?

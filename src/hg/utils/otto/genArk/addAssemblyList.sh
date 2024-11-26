#!/bin/bash

# exit on any error
set -beEu -o pipefail

cd /hive/data/inside/GenArk

export YYYY=`date "+%Y"`
export DS=`date "+%F"`
export dayOfMonth=`date "+%d" |  sed -e 's/^0//;'`

mkdir -p history/${YYYY}

time (./assemblyList.py dbDb.clade.year.acc.tsv) > history/${YYYY}/asmList${DS}.log 2>&1

rm -f beforeSort.prio
mv assemblyList.tsv beforeSort.asmList
sort -k2,2n beforeSort.asmList > assemblyList.tsv

hgsql hgcentraltest -e 'DROP TABLE IF EXISTS assemblyList;'
hgsql hgcentraltest < /hive/data/inside/GenArk/assemblyList.sql
hgsql hgcentraltest -e "LOAD DATA LOCAL INFILE 'assemblyList.tsv' INTO TABLE assemblyList;"

hgsql hgcentraltest -e 'select count(*) from assemblyList;' >> history/${YYYY}/asmList${DS}.log 2>&1

sort genark.tsv | join -t$'\t' - <(sort assemblyList.tsv | cut -f1-2,6) \
  | sort -t$'\t' -k7n > newGenark.tsv

hgsql hgcentraltest -e 'DROP TABLE IF EXISTS genark;'
hgsql hgcentraltest < /hive/data/inside/GenArk/newGenark.sql
hgsql hgcentraltest -e "LOAD DATA LOCAL INFILE 'newGenark.tsv' INTO TABLE genark;"

hgsql hgcentraltest -e 'SELECT COUNT(*) FROM genark;' >> history/${YYYY}/asmList${DS}.log 2>&1

# once a month (in the first week) archive the assemblyList.tsv
if [ "${dayOfMonth}" -lt 8 ]; then
  mkdir -p history/${YYYY}
  mv assemblyList.tsv history/${YYYY}/assemblyList.tsv.${DS}
  rm -f history/${YYYY}/assemblyList.tsv.${DS}.gz
  gzip history/${YYYY}/assemblyList.tsv.${DS}
fi

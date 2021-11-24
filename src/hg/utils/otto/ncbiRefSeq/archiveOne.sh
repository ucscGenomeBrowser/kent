#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: archiveOne.sh <wrkDir>\n" 1>&2
  printf "where <wrkDir> is something like:\n\t/hive/data/genomes/hg38/bed/ncbiRefSeq.p13.2021-05-28\n" 1>&2
  exit 255
fi

export wrkDir=$1
export db=`echo $wrkDir | cut -d'/' -f5`
export version=`awk '{print $(NF-1)}' ${wrkDir}/process/ncbiRefSeqVersion.txt`
export dumpDir="${wrkDir}/archive"
export arkDir="/usr/local/apache/htdocs-hgdownload/goldenPath/archive/$db/ncbiRefSeq/$version"

printf "#      db: %s\n" "${db}" 1>&2
printf "# version: %s\n" "${version}" 1>&2
printf "#  arkDir: %s\n" "${arkDir}" 1>&2
printf "# dumpDir: %s\n" "${dumpDir}" 1>&2

mkdir -p "${dumpDir}"

for tbl in extNcbiRefSeq ncbiRefSeq ncbiRefSeqCds ncbiRefSeqCurated \
	ncbiRefSeqLink ncbiRefSeqOther ncbiRefSeqPepTable ncbiRefSeqPsl \
	seqNcbiRefSeq ncbiRefSeqHgmd ncbiRefSeqSelect
do
  printf "# %s.%s\n" "${db}" "${tbl}" 1>&2
  hgsqldump --no-data "${db}" "${tbl}" > "${dumpDir}/${db}.${tbl}.sql"
  hgsql -N "${db}" -e "select * from ${tbl};" | gzip -c > "${dumpDir}/${db}.${tbl}.txt.gz"
done

for file in ncbiRefSeqOther.bb ncbiRefSeqOther.ix ncbiRefSeqOther.ixx \
        ncbiRefSeqVersion.txt seqNcbiRefSeq.rna.fa ncbiRefSeqGenomicDiff.bb
do
  printf "# gbdb %s %s\n" "${db}" "${file}" 1>&2
  cp -p "/gbdb/${db}/ncbiRefSeq/${file}" "${dumpDir}/gbdb.${db}.${file}"
done
printf "# gzip gbdb.%s.seqNcbiRefSeq.rna.fa\n" "${db}" 1>&2
gzip "${dumpDir}/gbdb.${db}.seqNcbiRefSeq.rna.fa"

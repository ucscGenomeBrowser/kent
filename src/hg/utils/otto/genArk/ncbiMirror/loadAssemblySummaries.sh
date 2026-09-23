#!/usr/bin/env bash
# Load the assembly_summary reports into MySQL database 'genark'
# using the assemblySummary.sql definition in the source tree.
# Usage: ./loadAssemblySummaries.sh [GCA|GCF]
#  used by the 'reports/fetch.sh' cron script
set -euo pipefail

if [ $# -ne 1 ]; then
  printf "usage: loadAssemblySummaries.sh [GCA|GCF]\n" 1>&2
  exit 255
fi

export type="${1}"

REPORTS_DIR=/hive/data/outside/ncbi/genomes/reports
SQL_DEF=${HOME}/kent/src/hg/lib/assemblySummary.sql
DB=genark
WORKDIR=/dev/shm/aSummary.$$
mkdir -p "${WORKDIR}"
rm -rf ${WORKDIR}/*

cleanTab() {
  # drop the 2 header/comment lines, drop any row that doesn't have exactly
  # 38 fields (the handful of genbank rows with an embedded literal tab),
  # and turn NCBI's 'na' missing-value marker -- and genuinely blank fields,
  # e.g. annotationDate for un-annotated assemblies -- into MySQL's LOAD DATA
  # NULL marker (\N) so hgLoadSqlTab loads real NULLs instead of 'na' or ''.
  awk -F'\t' -v OFS='\t' '!/^#/ && NF==38 {
      for (i=1;i<=NF;i++) if ($i=="na" || $i=="") $i="\\N"
      print
  }' "$1"
}

case $type in
  GCA)
    cleanTab "${REPORTS_DIR}/assembly_summary_genbank.txt"            > "${WORKDIR}/genbank.tab"
    cleanTab "${REPORTS_DIR}/assembly_summary_genbank_historical.txt" > "${WORKDIR}/genbankHistorical.tab"
    /cluster/bin/x86_64/hgLoadSqlTab ${DB} assemblySummaryGenbank           ${SQL_DEF} "${WORKDIR}/genbank.tab"
    /cluster/bin/x86_64/hgLoadSqlTab ${DB} assemblySummaryGenbankHistorical ${SQL_DEF} "${WORKDIR}/genbankHistorical.tab"
    ;;
  GCF)
    cleanTab "${REPORTS_DIR}/assembly_summary_refseq.txt"             > "${WORKDIR}/refseq.tab"
    cleanTab "${REPORTS_DIR}/assembly_summary_refseq_historical.txt"  > "${WORKDIR}/refseqHistorical.tab"
    /cluster/bin/x86_64/hgLoadSqlTab ${DB} assemblySummaryRefseq            ${SQL_DEF} "${WORKDIR}/refseq.tab"
    /cluster/bin/x86_64/hgLoadSqlTab ${DB} assemblySummaryRefseqHistorical  ${SQL_DEF} "${WORKDIR}/refseqHistorical.tab"
    ;;
esac

# -notOnServer: WORKDIR is a client-side mktemp dir, not necessarily visible
# to the mysqld process -- drop this flag if your db host shares the filesystem.

rm -fr "${WORKDIR}"

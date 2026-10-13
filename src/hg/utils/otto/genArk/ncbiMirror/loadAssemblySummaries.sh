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
HGSQL=/cluster/bin/x86_64/hgsql
HGLOAD=/cluster/bin/x86_64/hgLoadSqlTab
WORKDIR=/dev/shm/aSummary.$$
mkdir -p "${WORKDIR}"
rm -rf ${WORKDIR}/*
# clean up /dev/shm on any exit, including failure
trap 'rm -rf "${WORKDIR}"' EXIT

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

rowCount() {
  ${HGSQL} -N ${DB} -e "select count(*) from $1;"
}

# abort, leaving the live tables untouched, when the new table did not
# load every row of its tab file, or is more than 1% smaller than the
# live table.  The NCBI files do shrink a little now and then: a
# suppressed assembly leaves the current file, a reinstated or removed
# one leaves the historical file.  A bigger drop is more likely a
# truncated download or a format change.
checkRowCount() {
  local table="$1" tabFile="$2"
  local oldCount newCount tabCount
  oldCount=$(rowCount "${table}")
  newCount=$(rowCount "${table}New")
  tabCount=$(wc -l < "${tabFile}")
  if [ "${newCount}" -ne "${tabCount}" ]; then
    printf "ERROR: %s.%sNew has %s rows, its tab file has %s lines, not swapping\n" \
      "${DB}" "${table}" "${newCount}" "${tabCount}" 1>&2
    exit 255
  fi
  if [ $(( newCount * 100 )) -lt $(( oldCount * 99 )) ]; then
    printf "ERROR: %s.%sNew has %s rows, more than 1%% fewer than %s in %s.%s, not swapping\n" \
      "${DB}" "${table}" "${newCount}" "${oldCount}" "${DB}" "${table}" 1>&2
    exit 255
  fi
  if [ "${newCount}" -lt "${oldCount}" ]; then
    printf "# %s.%s: %s rows, %s fewer than before\n" \
      "${DB}" "${table}" "${newCount}" $(( oldCount - newCount ))
  fi
}

# the rows cleanTab() leaves out, not 38 fields; when NCBI adds or
# removes a column every row is left out, stop before loading nothing
checkTab() {
  local source="$1" tabFile="$2"
  local dropped
  dropped=$(awk -F'\t' '!/^#/ && NF!=38' "${source}" | wc -l)
  if [ ! -s "${tabFile}" ]; then
    printf "ERROR: no rows with 38 fields in %s, did NCBI change the columns ?\n" \
      "${source}" 1>&2
    exit 255
  fi
  if [ "${dropped}" -gt 0 ]; then
    printf "# %s: %s rows without 38 fields left out\n" "${source}" "${dropped}"
  fi
}

# Load both tab files into <table>New tables, then swap both pairs into
# place with a single RENAME so readers never see a missing/partial table,
# and the current and historical tables change together.
loadAndSwap() {
  local cur="$1" curTab="$2" hist="$3" histTab="$4"
  ${HGLOAD} ${DB} "${cur}New"  ${SQL_DEF} "${curTab}"
  ${HGLOAD} ${DB} "${hist}New" ${SQL_DEF} "${histTab}"
  # first-ever run: give RENAME something to move aside
  ${HGSQL} ${DB} -e "CREATE TABLE IF NOT EXISTS ${cur}  LIKE ${cur}New;
                     CREATE TABLE IF NOT EXISTS ${hist} LIKE ${hist}New;"
  checkRowCount "${cur}" "${curTab}"
  checkRowCount "${hist}" "${histTab}"
  ${HGSQL} ${DB} -e "DROP TABLE IF EXISTS ${cur}Old, ${hist}Old;"
  ${HGSQL} ${DB} -e "RENAME TABLE ${cur}  TO ${cur}Old,  ${cur}New  TO ${cur},
                                  ${hist} TO ${hist}Old, ${hist}New TO ${hist};"
  ${HGSQL} ${DB} -e "DROP TABLE ${cur}Old, ${hist}Old;"
}

case $type in
  GCA)
    cleanTab "${REPORTS_DIR}/assembly_summary_genbank.txt"            > "${WORKDIR}/genbank.tab"
    cleanTab "${REPORTS_DIR}/assembly_summary_genbank_historical.txt" > "${WORKDIR}/genbankHistorical.tab"
    checkTab "${REPORTS_DIR}/assembly_summary_genbank.txt" "${WORKDIR}/genbank.tab"
    checkTab "${REPORTS_DIR}/assembly_summary_genbank_historical.txt" "${WORKDIR}/genbankHistorical.tab"
    loadAndSwap assemblySummaryGenbank           "${WORKDIR}/genbank.tab" \
                assemblySummaryGenbankHistorical "${WORKDIR}/genbankHistorical.tab"
    ;;
  GCF)
    cleanTab "${REPORTS_DIR}/assembly_summary_refseq.txt"             > "${WORKDIR}/refseq.tab"
    cleanTab "${REPORTS_DIR}/assembly_summary_refseq_historical.txt"  > "${WORKDIR}/refseqHistorical.tab"
    checkTab "${REPORTS_DIR}/assembly_summary_refseq.txt" "${WORKDIR}/refseq.tab"
    checkTab "${REPORTS_DIR}/assembly_summary_refseq_historical.txt" "${WORKDIR}/refseqHistorical.tab"
    loadAndSwap assemblySummaryRefseq            "${WORKDIR}/refseq.tab" \
                assemblySummaryRefseqHistorical  "${WORKDIR}/refseqHistorical.tab"
    ;;
esac

#!/bin/bash
# Run hprc2ProcessOne.sh over all HPRC2 assemblies in parallel.
# Redmine #35415
# Usage: hprc2ProcessAll.sh <jobs.tsv> <bedDir> [jobs]
#   jobs.tsv: rawChainPath <TAB> shortName  (one per line)

set -beEu -o pipefail
export PATH=/usr/bin:/bin:/cluster/bin/x86_64:/cluster/bin/scripts:$PATH

jobsTsv="${1}"
BED="${2}"
jobs="${3:-16}"
SC="$HOME/kent/src/hg/makeDb/scripts/hprc2"

mkdir -p "${BED}/procLogs"
parallel -j "${jobs}" --colsep '\t' --bar --joblog "${BED}/procLogs/joblog.txt" \
  "bash ${SC}/hprc2ProcessOne.sh {1} {2} ${BED} > ${BED}/procLogs/{2}.log 2>&1" \
  :::: "${jobsTsv}"

echo "processed $(ls ${BED}/chain/*.chain | wc -l) assemblies"

#!/bin/bash
# Run hprc2XIndelsOne.sh over all HPRC2 assemblies in parallel to rebuild the
# left-normalized indel intermediates for the hprc2X track set.
# Redmine #35415
# Usage: hprc2XIndelsAll.sh <hprc2Dir> <hprc2XDir> [jobs]

set -beEu -o pipefail
export PATH=/usr/bin:/bin:$HOME/bin/x86_64:/cluster/bin/x86_64:/cluster/bin/scripts:$PATH

SRC="${1}"
BED="${2}"
jobs="${3:-16}"
SC="$HOME/kent/src/hg/makeDb/scripts/hprc2X"

mkdir -p "${BED}/procLogs"
# assembly short names come from the chains already processed in the hprc2 workdir
ls "${SRC}"/chain/*.chain | xargs -n1 basename | sed 's/\.chain$//' > "${BED}/names.txt"

parallel -j "${jobs}" --bar --joblog "${BED}/procLogs/joblog.txt" \
  "bash ${SC}/hprc2XIndelsOne.sh {} ${SRC} ${BED} > ${BED}/procLogs/{}.log 2>&1" \
  :::: "${BED}/names.txt"

echo "regenerated indels for $(ls ${BED}/arr/*.indel.txt | wc -l) assemblies"

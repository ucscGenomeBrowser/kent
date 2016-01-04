#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: ucscPatchNames.sh [pathTo/assembly_report.txt] > ucsc.to.ncbi.patch.names\n" 1>&2
  exit 255
fi

export asmRpt=$1

(grep -v "^#" "${asmRpt}" || true) | awk '{
if (match($2,".*patch")) {
 suffix="_fix"
 if (match($2,"novel.*")) { suffix="_alt" }
 ucscName=$5
 sub("\\.","v",ucscName)
 printf "chr%s_%s%s\t%s\n", $3, ucscName, suffix, $7
 }
}
'

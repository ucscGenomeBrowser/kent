#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: ucscAltNames.sh [pathTo/assembly_report.txt] > ucsc.to.ncbi.alt.names\n" 1>&2
  exit 255
fi

export asmRpt=$1

(grep -v "^#" "${asmRpt}" || true) | awk '{
if (match($2,"alt-scaffold")) {
 ucscName=$5
 sub("\\.","v",ucscName)
 printf "chr%s_%s_alt\t%s\n", $3, ucscName, $7
 }
}
'


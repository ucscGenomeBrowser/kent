#!/bin/bash

set -beEu -o pipefail

if [ $# -lt 2 ]; then
  echo "usage: scoreScan.sh <target> <query>" 1>&2
  exit 255
fi

export target=$1
export query=$2

~/kent/src/hg/utils/automation/lastz_D/mafScoreSizeScan.pl \
    ${target}.${query}.oneOff.maf > mafScoreSizeScan.list
ave mafScoreSizeScan.list | grep "^Q3" | awk '{print $2}' \
    | sed -e 's/.000000//' > mafScoreSizeScan.Q3


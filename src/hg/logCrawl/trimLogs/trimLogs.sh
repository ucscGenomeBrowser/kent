#!/bin/bash
set -beEu -o pipefail
export fname=$1
export result=$2

mkdir -p `dirname $result`
if [[ ! -e "$result" || ! -s "$result" ]]; then
    /cluster/home/chmalee/bin/scripts/trimTrackLogs ${fname} | gzip -c > ${result}
fi

#!/bin/bash
# Run httpsFerryLeakTest in the background and report its thread count and
# open socket count every few seconds until it exits.
#
# usage: httpsFerryLeakTest.sh [URL] [interval] [httpsFerryLeakTest options]
#   httpsFerryLeakTest.sh
#   httpsFerryLeakTest.sh 10 -close -sleep=120
#   httpsFerryLeakTest.sh https://host/big.bb 10 -close
#
# URL defaults to a 1.4 GB vcf.gz on hgwdev. interval is seconds between
# reports (default 15).
#
# httpsFerryLeakTest options:
#   -readSize=N  bytes to read before the errAbort (default 4096)
#   -sleep=N     seconds to sleep after the errCatch (default 180)
#   -close       call udcFileClose before the errAbort. Without it the udc file
#                is left open, and the https thread and its socket to the remote
#                host stay alive for the whole sleep. With it they should go away
#                right after the close.

set -u

if [ "$(hostname -s)" != "hgwdev" ]; then
    echo "$0: only runs on hgwdev" >&2
    exit 1
fi

url=https://hgwdev.gi.ucsc.edu/~chmalee/vcfExampleCTs/evaSnps.ucscChroms.vcf.gz
if [ $# -gt 0 ] && [[ $1 == https://* ]]; then
    url=$1
    shift
fi
interval=15
if [ $# -gt 0 ] && [[ $1 =~ ^[0-9]+$ ]]; then
    interval=$1
    shift
fi

prog=$(dirname "$0")/bin/$(uname -m)/httpsFerryLeakTest
if [ ! -x "$prog" ]; then
    echo "$prog not found, run: make httpsFerryLeakTest" >&2
    exit 1
fi

"$prog" "$@" "$url" &
pid=$!
trap 'kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null; echo "killed pid $pid"; exit 130' INT TERM
start=$(date +%s)
echo "started pid $pid"

while kill -0 "$pid" 2>/dev/null; do
    now=$(( $(date +%s) - start ))
    threads=$(ls /proc/$pid/task 2>/dev/null | wc -l)
    sockets=$(ls -l /proc/$pid/fd 2>/dev/null | grep -c socket)
    echo "=== t=${now}s threads=$threads sockets=$sockets"
    sleep "$interval"
done

wait "$pid"
echo "pid $pid exited with status $?"

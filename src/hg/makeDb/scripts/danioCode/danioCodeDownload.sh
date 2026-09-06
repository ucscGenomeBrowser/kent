#!/bin/bash
# Mirror the DANIO-CODE data files listed in a URL file into an output directory.
# Resumes partial files, skips files that are already complete, and prints the
# URLs that failed so they can be retried.
#
#   danioCodeDownload.sh <urlListFile> <outDir> [jobs]
#
# 2026-09-04, Claude + Max.
set -o pipefail
urlFile=$1
outDir=$2
jobs=${3:-10}
[ -z "$outDir" ] && { echo "usage: $0 <urlListFile> <outDir> [jobs]" >&2; exit 1; }
mkdir -p "$outDir"

fetch() {
    local url=$1 out=$2
    local f="$out/$(basename "$url")"
    # remote size, so a complete file is not fetched again
    local remote
    remote=$(curl -sIL --max-time 60 "$url" \
             | awk 'BEGIN{IGNORECASE=1} /^content-length:/{n=$2} END{gsub(/\r/,"",n); print n+0}')
    if [ -s "$f" ] && [ "$(stat -c %s "$f")" = "$remote" ]; then
        return 0
    fi
    curl -sSfL -C - --retry 3 --retry-delay 5 --max-time 7200 -o "$f" "$url" || {
        echo "FAILED $url" >&2
        return 1
    }
    local got=$(stat -c %s "$f" 2>/dev/null || echo 0)
    if [ "$remote" -gt 0 ] && [ "$got" != "$remote" ]; then
        echo "SHORT $url got=$got want=$remote" >&2
        return 1
    fi
}
export -f fetch

parallel -j "$jobs" fetch {} "$outDir" :::: "$urlFile"

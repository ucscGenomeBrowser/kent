#!/bin/bash
#
# Cron entry point for the UMD TP53 variant track. Designed for silent
# operation under cron: stdout/stderr stay empty whenever nothing needs to
# happen, so no mail is sent. Routine progress messages go to a dated log
# file. Only genuine errors propagate to stderr (and therefore to cron mail).
#
# Do not modify this script in /hive/data/outside/otto/umdTp53; edit the kent
# source at src/hg/utils/otto/umdTp53/doUmdTp53Update.sh and `make install`.

set -o errexit -o pipefail -o nounset
umask 002

WORKDIR="/hive/data/outside/otto/umdTp53"
LOGDIR="${WORKDIR}/log"
mkdir -p "${LOGDIR}"
LOGFILE="${LOGDIR}/umdTp53.$(date +%F).log"

cd "${WORKDIR}"

# Step 1: download (silent on no-op; prints dated workdir on real change).
# Capture stdout into a marker; route stderr to the log so curl diagnostics
# are visible if something fails.
if ! marker=$(./download.sh 2>>"${LOGFILE}"); then
    echo "ERROR: umdTp53 download.sh failed; see ${LOGFILE}" 1>&2
    exit 1
fi

if [ -z "${marker}" ]; then
    # Nothing changed. Stay silent.
    exit 0
fi

# Step 2: build + install. Everything goes to the log; only a non-zero exit
# bubbles out and triggers cron mail.
{
    echo "=== umdTp53 build $(date -Iseconds) on ${marker} ==="
    if ! ./checkAndLoad.sh "${marker}"; then
        echo "ERROR: umdTp53 checkAndLoad.sh failed on ${marker}" 1>&2
        exit 1
    fi
    echo "=== umdTp53 build complete $(date -Iseconds) ==="
} >>"${LOGFILE}" 2>&1

#!/bin/bash
# checkRefSeqHistoricalUpdate.sh -- weekly notifier (RM #35766).
# Emails the otto MAILTO ONLY if NCBI has published a GRCh38 RefSeq historical
# release newer than the one we've already handled. Silent (no output, so cron
# sends no mail) when there is nothing new.
#
# NCBI names each release dir GCF_000001405.40-RS_YYYY_MM_historical/, and the
# RS_YYYY_MM tag sorts lexically = chronologically.
#
# To silence after updating the track: set the new tag in lastHandledRelease.txt.
# Build steps live in the RS_YYYY_MM sections of:
#   kent/src/hg/makeDb/doc/hg38/ncbiRefSeq.txt
set -o pipefail
cd /hive/data/outside/otto/refSeqHistorical || exit 1

baseUrl="https://ftp.ncbi.nlm.nih.gov/refseq/H_sapiens/historical/GRCh38/"
handled=$(tr -d '[:space:]' < lastHandledRelease.txt 2>/dev/null)

# NCBI's FTP/HTTPS server fails sporadically; retry a few times before giving
# up, so a transient blip does not turn into a false "couldn't reach" mail.
listing=""
for attempt in 1 2 3 4 5; do
    listing=$(wget -q -O - "$baseUrl") && [ -n "$listing" ] && break
    listing=""
    sleep 30
done

if [ -z "$listing" ]; then
    echo "refSeqHistorical check: could not reach $baseUrl after 5 attempts"
    exit 1
fi

latest=$(echo "$listing" | grep -oE 'RS_[0-9]{4}_[0-9]{2}' | sort -u | tail -1)
if [ -z "$latest" ]; then
    echo "refSeqHistorical check: no RS_* release tags found at $baseUrl (page format changed?)"
    exit 1
fi

if [[ "$latest" > "$handled" ]]; then
    echo "New RefSeq historical release available for hg38: $latest (we have: ${handled:-none})"
    echo "Source: $baseUrl"
    echo "Build steps: RS_YYYY_MM section in kent/src/hg/makeDb/doc/hg38/ncbiRefSeq.txt"
    echo "To silence: set $latest in /hive/data/outside/otto/refSeqHistorical/lastHandledRelease.txt. refs #35766"
fi
exit 0

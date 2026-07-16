#!/bin/bash
# Download the 312 ENCODE bulk RNA-seq bigWigs for the mouse developmental
# time course into /hive/data/outside/woldlab/mouseDevTimecourse/mm10/bigwig/.
# Reads URLs from Diane's biosample TSV (#36998 attachment).
#
# Designed to be safely restartable: skips files that already exist.
# Run inside screen so it survives session disconnects:
#   screen -dmS bigwig_download ~/kent/src/hg/makeDb/scripts/mouseDevTimecourse/downloadBigwigs.sh

set -u

TSV=/hive/data/outside/woldlab/mouseDevTimecourse/mm10/ENCSR574CRQ_biosample.tsv
DEST=/hive/data/outside/woldlab/mouseDevTimecourse/mm10/bigwig
LOG=/hive/data/outside/woldlab/mouseDevTimecourse/mm10/download_bigwigs.log

mkdir -p "$DEST"
: > "$LOG"

echo "$(date) start" | tee -a "$LOG"
echo "TSV: $TSV"   | tee -a "$LOG"
echo "DEST: $DEST" | tee -a "$LOG"

# Extract URLs from columns 7 (signal_of_unique_reads) and 8 (signal_of_all_reads),
# skip the header line. One URL per output line.
URLS=$(awk -F'\t' 'NR>1 {print $7; print $8}' "$TSV")
TOTAL=$(echo "$URLS" | wc -l)
echo "$TOTAL URLs to fetch" | tee -a "$LOG"

i=0
for URL in $URLS; do
    i=$((i + 1))
    NAME=$(basename "$URL")
    OUT="$DEST/$NAME"
    if [ -s "$OUT" ]; then
        echo "[$i/$TOTAL] skip (exists): $NAME" >> "$LOG"
        continue
    fi
    echo "[$i/$TOTAL] fetching: $NAME" >> "$LOG"
    curl -sSL -o "$OUT.partial" "$URL"
    rc=$?
    if [ $rc -eq 0 ]; then
        mv "$OUT.partial" "$OUT"
    else
        echo "[$i/$TOTAL] FAILED rc=$rc: $URL" >> "$LOG"
        rm -f "$OUT.partial"
    fi
done

echo "$(date) done" | tee -a "$LOG"

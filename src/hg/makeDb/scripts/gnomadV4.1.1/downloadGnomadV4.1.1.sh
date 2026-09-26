#!/bin/bash
# downloadGnomadV4.1.1.sh -- mirror per-chromosome gnomAD v4.1.1 sites VCFs from
# the public GCS bucket into /hive/data/outside/gnomAD.4.1.1/{exomes,genomes}/.
#
# Usage: downloadGnomadV4.1.1.sh exomes|genomes [parallelism]
#
# Pulls chr1..22, chrX, chrY (no chrM in this release). Per-chromosome
# .vcf.bgz plus .tbi sibling. Uses GNU parallel; default 0 = no cap
# (all 48 files at once -- public GCS bucket handles it fine). Writes
# urls.txt (the work list), parallel.joblog (parallel's per-job timing
# and exit code), and SUMMARY to download.log next to the data.

set -u

if [ $# -lt 1 ] || [ $# -gt 2 ] || { [ "$1" != "exomes" ] && [ "$1" != "genomes" ]; }; then
    echo "usage: $0 exomes|genomes [parallelism]" >&2
    exit 2
fi
DATASET="$1"
JOBS="${2:-0}"

DEST=/hive/data/outside/gnomAD.4.1.1/${DATASET}
BASE_URL="https://storage.googleapis.com/gcp-public-data--gnomad/release/4.1.1/vcf/${DATASET}"
URLS=${DEST}/urls.txt
JOBLOG=${DEST}/parallel.joblog
LOG=${DEST}/download.log

mkdir -p "$DEST"
rm -f "$URLS" "$JOBLOG" "$LOG"

CHROMS="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y"

for chr in $CHROMS; do
    echo "${BASE_URL}/gnomad.${DATASET}.v4.1.1.sites.chr${chr}.vcf.bgz"     >> "$URLS"
    echo "${BASE_URL}/gnomad.${DATASET}.v4.1.1.sites.chr${chr}.vcf.bgz.tbi" >> "$URLS"
done

# -c lets parallel reruns resume partial files; -nv keeps the log tight;
# -P "$DEST" puts each download into the right directory while keeping
# the filename from the URL.
parallel --joblog "$JOBLOG" -j "$JOBS" \
    wget -c -nv -t 5 --waitretry=30 -P "$DEST" {} \
    :::: "$URLS"

# Tally from parallel's joblog (column 7 == exit status of each job).
ok=$(awk 'NR>1 && $7==0' "$JOBLOG" | wc -l)
fail=$(awk 'NR>1 && $7!=0' "$JOBLOG" | wc -l)
printf 'SUMMARY %s: ok=%d fail=%d total=%d\n' "$DATASET" "$ok" "$fail" $((ok+fail)) | tee "$LOG"

[ "$fail" -eq 0 ]

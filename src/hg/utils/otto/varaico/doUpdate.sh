#!/bin/bash
# OTTO update for the Varaico tracks (varaico, varaicoSuppl) on hg19, hg38 and hs1.
#
# Installs the newest release staged under /hive/users/jbirgmei/TRACK_VERSIONS/
# that carries a READY marker. READY holds the public version string, e.g.
# "release 5 (20270301)", which becomes /gbdb/$db/bbi/varaicoVersion.txt.
# Exits quietly when that release is already installed (currentRelease.txt).
#
# Usage: ./doUpdate.sh [--dry-run]   (--dry-run: run the checks, change nothing)
# See README for the staging layout. Redmine #38543.
umask 002
set -euo pipefail
export PATH=/cluster/bin/x86_64:/usr/bin:/bin

# the VARAICO_* overrides exist only for testing in a scratch tree
OTTO_DIR=${VARAICO_OTTO_DIR:-/hive/data/outside/otto/varaico}
STAGE=${VARAICO_STAGE:-/hive/users/jbirgmei/TRACK_VERSIONS}
GENOMES=${VARAICO_GENOMES:-/hive/data/genomes}
ARCHIVE=${VARAICO_ARCHIVE:-/usr/local/apache/htdocs-hgdownload/goldenPath/archive}
GBDB=${VARAICO_GBDB:-/gbdb}
DBS=(hg19 hg38 hs1)
MAILTO=${VARAICO_MAILTO:-jbirgmei@ucsc.edu}
MAX_ITEM_CHANGE_PCT=20

dry_run=false
if [ "${1:-}" = "--dry-run" ]; then dry_run=true; fi

cd "$OTTO_DIR"
log_dir=logs/$(date +%Y/%m)
mkdir -p "$log_dir"
LOG=$(readlink -f "$log_dir/$(date +%Y%m%d_%H%M%S).log")
exec > "$LOG" 2>&1

on_error() {
    echo "FAILED at line $1"
    mail -s "varaico OTTO update FAILED - $(date +%F)" "$MAILTO" < "$LOG"
    exit 1
}
trap 'on_error $LINENO' ERR

# source file in a staged release dir -> installed track file name
declare -A SRC_OF=(
    [varaico]=normalized_variants
    [varaicoSuppl]=normalized_variants_supplements
)

current=$(cat currentRelease.txt)
newest=""
for d in $(ls -d "$STAGE"/v*_* 2>/dev/null | sort -V); do
    if [ -s "$d/READY" ]; then newest=$(basename "$d"); fi
done
if [ -z "$newest" ] || [ "$newest" = "$current" ]; then
    echo "nothing new: newest READY release '${newest:-none}', installed '$current'"
    exit 0
fi
new_version=$(head -1 "$STAGE/$newest/READY")
echo "new release $newest ('$new_version'), replacing $current"

# --- sanity checks on every file before anything is touched
for db in "${DBS[@]}"; do
    for t in varaico varaicoSuppl; do
        src=$STAGE/$newest/$db/${SRC_OF[$t]}.$db.bb
        cur=$GENOMES/$db/bed/varaico/$t.bb
        new_n=$(bigBedInfo "$src" | awk '/^itemCount:/{gsub(",","",$2); print $2}')
        old_n=$(bigBedInfo "$cur" | awk '/^itemCount:/{gsub(",","",$2); print $2}')
        change=$(( (new_n - old_n) * 100 / old_n ))
        echo "$db $t: $old_n -> $new_n items (${change}%)"
        if [ "${change#-}" -gt "$MAX_ITEM_CHANGE_PCT" ]; then
            echo "item count changed by more than ${MAX_ITEM_CHANGE_PCT}%; refusing (install by hand if intended)"
            false
        fi
        unknown=$(comm -23 <(bigBedInfo -chroms "$src" | awk '/^\t/{print $1}' | sort) \
                           <(cut -f1 "$GENOMES/$db/chrom.sizes" | sort))
        if [ -n "$unknown" ]; then
            echo "chroms not in $db chrom.sizes: $unknown"
            false
        fi
    done
done
if $dry_run; then
    echo "dry run: checks passed, nothing changed"
    exit 0
fi

# --- archive the current release, then install the new one
for db in "${DBS[@]}"; do
    bed=$GENOMES/$db/bed/varaico
    mkdir -p "$GENOMES/$db/bed/varaico.$current" "$ARCHIVE/$db/varaico/$current"
    for t in varaico varaicoSuppl; do
        arch=$GENOMES/$db/bed/varaico.$current/$db.$t.$current.bb
        cp -p "$bed/$t.bb" "$arch"
        cmp "$bed/$t.bb" "$arch"
        ln -sfn "$arch" "$ARCHIVE/$db/varaico/$current/$db.$t.$current.bb"
    done
    for t in varaico varaicoSuppl; do
        src=$STAGE/$newest/$db/${SRC_OF[$t]}.$db.bb
        cp "$src" "$bed/.$t.bb.tmp"
        cmp "$src" "$bed/.$t.bb.tmp"
        chmod 664 "$bed/.$t.bb.tmp"
        mv -f "$bed/.$t.bb.tmp" "$bed/$t.bb"
    done
    echo "$new_version" > "$bed/.varaicoVersion.txt.tmp"
    chmod 664 "$bed/.varaicoVersion.txt.tmp"
    mv -f "$bed/.varaicoVersion.txt.tmp" "$bed/varaicoVersion.txt"
    for f in varaico.bb varaicoSuppl.bb varaicoVersion.txt; do
        ln -sfn "$bed/$f" "$GBDB/$db/bbi/$f"
    done
    echo "$db: installed $newest, archived $current"
done
echo "$newest" > currentRelease.txt
mail -s "varaico OTTO update: installed $newest ($new_version) - $(date +%F)" "$MAILTO" < "$LOG"

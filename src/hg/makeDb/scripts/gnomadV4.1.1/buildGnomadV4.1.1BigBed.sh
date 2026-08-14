#!/bin/bash
# buildGnomadV4.1.1BigBed.sh -- turn the per-chromosome gnomAD v4.1.1 sites VCFs
# into one bigBed (plus a bgzipped details file) per dataset, replicating the
# v4.1 pipeline documented in hg/makeDb/doc/hg38/gnomad.txt.
#
# Usage: buildGnomadV4.1.1BigBed.sh exomes|genomes [jobs]
#   jobs: parallelism for the per-chromosome CPU stages (default 20).
#
# Resumable: every stage skips work whose output already exists, so a rerun
# after a crash or logout-kill picks up where it left off. Long job (the
# genomes chain is ~12-18h on its own), so run it detached in screen, e.g.:
#   screen -dmS gnomadGen bash -c '.../buildGnomadV4.1.1BigBed.sh genomes'
#   tail -f /hive/data/inside/gnomAD/v4/v4.1.1/build.genomes.log
#
# v4.1.1 is format-compatible with v4.1 (same INFO fields, same VEP Format
# string), so the v4.1 field lists and the gnomadVcfBedToBigBed -v v4.1_*
# parser keys are reused unchanged.

set -uo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ] || { [ "$1" != "exomes" ] && [ "$1" != "genomes" ]; }; then
    echo "usage: $0 exomes|genomes [jobs]" >&2
    exit 2
fi
DS="$1"
JOBS="${2:-20}"
JOINJOBS=12                 # bedJoinTabOffset is memory-heavy; keep this modest

SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL="$SCRIPTDIR/../../gnomad/gnomadVcfBedToBigBed"
FIELDLIST="$SCRIPTDIR/v4.1.1.vcfToBed.$DS.fieldList"
AS="$SCRIPTDIR/$DS.as"
VER="v4.1_$DS"              # parser key; v4.1.1 reuses the v4.1 VEP/pop layout

WORKDIR=/hive/data/inside/gnomAD/v4/v4.1.1
IN=/hive/data/outside/gnomAD.4.1.1/$DS
SIZES=/hive/data/genomes/hg38/chrom.sizes
GBDB=/gbdb/hg38/gnomAD/v4.1.1/$DS
LOG="$WORKDIR/build.$DS.log"

CHROMS="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y"

# Intermediates and sort/parallel spill go on fast local /data/tmp scratch;
# only the final deliverables (.bb + details file) move to /hive at stage 7.
# The detached screen does not inherit the interactive TMPDIR, so set the
# scratch location explicitly rather than relying on the environment.
SCRATCH="/data/tmp/gnomadV4.1.1Build/$DS"
BEDDIR="$SCRATCH/hg38/$DS"          # per-chrom beds and tabs
JOINEDDIR="$SCRATCH/joined/$DS"     # per-chrom offset-joined beds
PRETAB="$SCRATCH/gnomad.v4.1.1.$DS.details.pre.tab"
JOINEDBED="$SCRATCH/gnomad.v4.1.1.$DS.joined.bed"
DETAILSGZ_S="$SCRATCH/gnomad.v4.1.1.$DS.details.tab.gz"   # built in scratch
DETAILSGZI_S="$DETAILSGZ_S.gzi"
BB_S="$SCRATCH/$DS.bb"                                    # built in scratch
# final /hive homes that the gbdb symlinks point at
DETAILSGZ="$WORKDIR/gnomad.v4.1.1.$DS.details.tab.gz"
DETAILSGZI="$DETAILSGZ.gzi"
BB="$WORKDIR/$DS.bb"

export TMPDIR="$SCRATCH/tmp"        # sort/parallel spill on fast local disk
mkdir -p "$BEDDIR" "$JOINEDDIR" "$TMPDIR" "$WORKDIR"
# send everything to the logfile and the terminal/screen both
exec > >(tee -a "$LOG") 2>&1

stageStart=0
log()   { echo "[$(date '+%F %T')] $*"; }
start() { stageStart=$SECONDS; log "=== STAGE $1 START ==="; }
done_() { log "=== STAGE $1 DONE ($(( (SECONDS-stageStart)/60 ))m$(( (SECONDS-stageStart)%60 ))s) ==="; }
die()   { log "ERROR: $*"; exit 1; }

# require that every chrom has a non-empty output named by a printf template
requireAll() {
    local tmpl="$1" missing=0 c f
    for c in $CHROMS; do
        f=$(printf "$tmpl" "$c")
        [ -s "$f" ] || { log "  missing/empty: $f"; missing=1; }
    done
    [ "$missing" -eq 0 ]
}

log "###### build $DS START (jobs=$JOBS) ######"
[ -s "$BB" ] && { log "$DS already published at $BB; nothing to do"; exit 0; }
[ -x "$TOOL" ] || die "gnomadVcfBedToBigBed not found/executable at $TOOL"
[ -s "$FIELDLIST" ] || die "field list missing: $FIELDLIST"
[ -s "$AS" ] || die "autosql missing: $AS"

# Stage 1: VCF -> one big bed per chrom. vcfToBed can exit nonzero yet still
# write a valid bed, so we judge success by the output, not the exit code.
start 1-vcfToBed
if requireAll "$BEDDIR/gnomad.v4.1.1.chr%s.bed"; then
    log "  all per-chrom beds present, skipping"
else
    JL="$WORKDIR/jobList.vcfToBed.$DS"
    : > "$JL"
    for c in $CHROMS; do
        out="$BEDDIR/gnomad.v4.1.1.chr$c.bed"
        [ -s "$out" ] && continue
        echo "vcfToBed -fields=$FIELDLIST -fieldsIsFile $IN/gnomad.$DS.v4.1.1.sites.chr$c.vcf.bgz $out"
    done >> "$JL"
    log "  running $(wc -l < "$JL") vcfToBed jobs at -j $JOBS"
    parallel --tmpdir "$TMPDIR" --joblog "$WORKDIR/jobLog.vcfToBed.$DS" -j "$JOBS" :::: "$JL" || true
    requireAll "$BEDDIR/gnomad.v4.1.1.chr%s.bed" || die "stage 1 left missing beds"
fi
# Completeness guard (always runs, including on resumed beds). A killed/truncated
# vcfToBed write (the out-of-disk kill that lost chr2's tail) silently yields a
# short bed. vcfToBed emits exactly one bed row per VCF record, so verify each
# bed is newline-terminated AND its data-row count equals the VCF record count
# (bgzip-decompress + count non-header lines). Counts are cached per chrom so
# resumes do not redo the full-file scan.
log "  counting VCF records (bgzip + grep) for the completeness check"
CJL="$WORKDIR/jobList.count.$DS"
: > "$CJL"
for c in $CHROMS; do
    cf="$SCRATCH/count.chr$c"
    [ -s "$cf" ] && continue
    # record count = non-header lines; faster than bcftools stats (no tallying)
    echo "/cluster/bin/x86_64/bgzip -cd $IN/gnomad.$DS.v4.1.1.sites.chr$c.vcf.bgz | grep -vc '^#' > $cf"
done >> "$CJL"
[ -s "$CJL" ] && parallel --tmpdir "$TMPDIR" -j "$JOBS" :::: "$CJL"
bad=0
for c in $CHROMS; do
    bed="$BEDDIR/gnomad.v4.1.1.chr$c.bed"
    [ -z "$(tail -c1 "$bed")" ] || { log "  chr$c: bed not newline-terminated -> truncated write"; bad=1; }
    data=$(( $(wc -l < "$bed") - $(head -1 "$bed" | grep -c '^#') ))
    rec=$(cat "$SCRATCH/count.chr$c" 2>/dev/null)
    [ "$data" = "$rec" ] || { log "  chr$c: bed data rows=$data but VCF records=$rec (MISMATCH)"; bad=1; }
done
[ "$bad" -eq 0 ] || die "stage 1 beds incomplete vs VCF record counts (see above)"
log "  all $DS beds complete: data rows == VCF records, newline-terminated"
done_ 1-vcfToBed

# Stage 2: split each big bed into a display bed + a details .tab (the parser
# knows the v4.1 layout via -v). Output bed is sorted for the later merge.
start 2-convertBeds
if requireAll "$BEDDIR/chr%s.bed" && requireAll "$BEDDIR/gnomad.v4.1.1.chr%s.$DS.tab"; then
    log "  all per-chrom converted beds + tabs present, skipping"
else
    JL="$WORKDIR/jobList.convertBeds.$DS"
    : > "$JL"
    for c in $CHROMS; do
        inbed="$BEDDIR/gnomad.v4.1.1.chr$c.bed"
        outbed="$BEDDIR/chr$c.bed"
        tab="$BEDDIR/gnomad.v4.1.1.chr$c.$DS.tab"
        { [ -s "$outbed" ] && [ -s "$tab" ]; } && continue
        raw="$BEDDIR/chr$c.raw.bed"
        # write to a file (not "| sort") so a tool crash is not masked by sort's exit
        echo "$TOOL --extra-output-file $tab -v $VER $inbed $raw && sort -T $TMPDIR -k1,1 -k2,2n $raw > $outbed && rm -f $raw"
    done >> "$JL"
    log "  running $(wc -l < "$JL") convert jobs at -j $JOBS"
    parallel --tmpdir "$TMPDIR" --joblog "$WORKDIR/jobLog.convertBeds.$DS" -j "$JOBS" :::: "$JL" \
        || die "stage 2 had failing convert jobs (see jobLog.convertBeds.$DS)"
    { requireAll "$BEDDIR/chr%s.bed" && requireAll "$BEDDIR/gnomad.v4.1.1.chr%s.$DS.tab"; } \
        || die "stage 2 left missing beds/tabs"
fi
done_ 2-convertBeds

# Stage 3: merge the per-chrom details tabs and bgzip with an index for the
# detail-page random access.
start 3-bgzipDetails
if [ -s "$DETAILSGZ_S" ] && [ -s "$DETAILSGZI_S" ]; then
    log "  $DETAILSGZ_S + index present, skipping"
else
    log "  merging per-chrom tabs -> $PRETAB"
    sort -T "$TMPDIR" --merge "$BEDDIR"/gnomad.v4.1.1.*."$DS".tab > "$PRETAB" || die "tab merge failed"
    log "  bgzip -> $DETAILSGZ_S"
    # full path: the PATH-resolved /cluster/software/bin/bgzip needs a libcrypto
    # that the detached-screen environment lacks; the htslib build does not
    /cluster/bin/x86_64/bgzip -iI "$DETAILSGZI_S" -c "$PRETAB" > "$DETAILSGZ_S" || die "bgzip failed"
fi
done_ 3-bgzipDetails

# Stage 4: append the byte offset/length of each variant's details line onto
# its display bed row. Needs the uncompressed PRETAB from stage 3.
start 4-bedJoinTabOffset
if requireAll "$JOINEDDIR/chr%s.bed"; then
    log "  all joined beds present, skipping"
else
    [ -s "$PRETAB" ] || die "PRETAB missing; rerun stage 3 (it is not kept after a clean)"
    JL="$WORKDIR/jobList.join.$DS"
    : > "$JL"
    for c in $CHROMS; do
        out="$JOINEDDIR/chr$c.bed"
        [ -s "$out" ] && continue
        echo "bedJoinTabOffset -verbose=2 -bedKey=3 $PRETAB $BEDDIR/chr$c.bed $out"
    done >> "$JL"
    log "  running $(wc -l < "$JL") join jobs at -j $JOINJOBS"
    parallel --tmpdir "$TMPDIR" --joblog "$WORKDIR/jobLog.join.$DS" -j "$JOINJOBS" :::: "$JL" \
        || die "stage 4 had failing join jobs (see jobLog.join.$DS)"
    requireAll "$JOINEDDIR/chr%s.bed" || die "stage 4 left missing joined beds"
fi
done_ 4-bedJoinTabOffset

# Stage 5: merge the joined per-chrom beds into one genome-wide bed.
start 5-sortMerge
if [ -s "$JOINEDBED" ]; then
    log "  $JOINEDBED present, skipping"
else
    sort -T "$TMPDIR" -S 40G --merge "$JOINEDDIR"/chr*.bed | grep -v "^#" > "$JOINEDBED" \
        || die "sort-merge failed"
fi
done_ 5-sortMerge

# Stage 6: build the bigBed (in scratch).
start 6-bedToBigBed
if [ -s "$BB_S" ]; then
    log "  $BB_S present, skipping"
else
    bedToBigBed -maxAlloc=250000000000 -type=bed9+21 -tab -as="$AS" \
        -extraIndex=name,rsId,_displayName \
        "$JOINEDBED" "$SIZES" "$BB_S" || die "bedToBigBed failed"
fi
# post-build check: one bigBed feature per joined-bed row (no silent drops)
items=$(bigBedInfo "$BB_S" 2>/dev/null | awk '/^itemCount:/{gsub(/,/,"",$2); print $2}')
bedrows=$(wc -l < "$JOINEDBED")
log "  bigBed itemCount=$items (joined bed rows=$bedrows)"
[ "$items" = "$bedrows" ] || die "bigBed itemCount ($items) != joined bed rows ($bedrows)"
done_ 6-bedToBigBed

# Stage 7: move the deliverables from fast scratch to their /hive home and
# publish via /gbdb symlinks (bigDataUrl never points into /hive directly,
# and never into the transient /data/tmp scratch).
start 7-publish
mkdir -p "$WORKDIR" "$GBDB"
log "  moving deliverables from $SCRATCH to $WORKDIR"
mv -f "$BB_S"         "$BB"         || die "move bb to /hive failed"
mv -f "$DETAILSGZ_S"  "$DETAILSGZ"  || die "move details.tab.gz to /hive failed"
mv -f "$DETAILSGZI_S" "$DETAILSGZI" || die "move details index to /hive failed"
ln -sf "$BB"         "$GBDB/$DS.bb"
ln -sf "$DETAILSGZ"  "$GBDB/gnomad.v4.1.1.$DS.details.tab.gz"
ln -sf "$DETAILSGZI" "$GBDB/gnomad.v4.1.1.$DS.details.tab.gz.gzi"
done_ 7-publish

log "###### BUILD COMPLETE $DS: $BB ($(du -h "$BB" | cut -f1)) ######"

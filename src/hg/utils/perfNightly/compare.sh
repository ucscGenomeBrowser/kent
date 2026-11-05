#!/bin/bash
# Compare two hgTracks builds on one machine: start-up time and pixels.  refs #37547
#
# The engine.  nightly.sh drives it for the cron; run it by hand to compare any two
# builds, for example genome-test against a branch:
#
#   ./compare.sh --a /usr/local/apache/cgi-bin --a-label genome-test \
#                --b ~/kentMergeVis/src/hg/hgTracks --b-label mergeVis
#
# Each side is a DIRECTORY holding both hgTracks and hgRenderTracks.  `make compile`
# in hg/hgTracks builds both and installs nothing, so a working tree is a valid side
# with no install step:  make -C src/hg/hgTracks compile -j8
#
# Why the binaries are run directly and not over HTTP
# ---------------------------------------------------
# Two builds reached over HTTP do not share a udcDir, a trackDb, or an hg.conf, and
# every one of those differences shows up as a code tax that is not in the code (see
# the bench-hgtracks-rts skill, which learned this on #37525: a +27% "narrow-view tax"
# vanished to parity once both builds ran against the same config).  Run directly,
# both sides get one hg.conf that includes production's, so the only difference left
# is the binary.
#
# Four things the direct path needs, each of which fails quietly if it is missing:
#
#   * a CWD you own, with a `trash` symlink beside it.  hgTracks writes its PNGs
#     relative to the CWD, and /usr/local/apache/cgi-bin/hgt is not group-writable,
#     so running there dies with "mustOpen: Can't open ./hgt/..." and returns a
#     plausible 37 KB page instead of an error.
#   * an `htdocs` symlink in the same place.  freetype loads
#     ../htdocs/urw-fonts/n019003l.pfb by a path relative to the CWD.  Without it
#     every render dies with a stack dump.  Passing textFont=Bitmap avoids it, but
#     then the pixels are not the ones production draws, so that is no use here.
#   * a cacheTrackDbDir PER SIDE.  TRACKDB_VERSION is baked into each cache dump's
#     filename, so two builds whose layout differs never read each other's dumps --
#     but the side whose version is new starts with an empty cache and pays a full
#     trackDb parse on every cell.  Measured 2026-09-15: /data/trackDbCache held
#     21,919 dumps at version 9 and none at version 10, which is what the #37547
#     branch writes.  A naive run would have reported that one-time cost as a
#     permanent regression.  Each side gets its own dir, and each side is warmed on
#     every cell before anything is timed.
#   * a cart PER SIDE.  Two builds that disagree about cart format cannot share one:
#     on #37547 the branch's cartSetVisString() deletes the legacy bare key, so the
#     other build reads nothing and silently draws trackDb defaults -- a fast, wrong
#     render that no timing check can see.  The trackLog assertion below is what
#     catches it.
#
# What is asserted, and why it is trackLog and not a row count
# -----------------------------------------------------------
# hgTracks writes `trackLog <n> <db> <hgsid> name:vis,name:vis,...` to stderr.  That
# is the exact set of tracks it drew and at what visibility.  The two sides must
# produce the same set.  A row count cannot see a track that came up dense instead of
# pack, and a page that failed to load its session still counts rows happily.
#
# Cold and warm are different measurements and both are reported
# -------------------------------------------------------------
# A one-shot render gives every request a fresh cart, so it measures only the cold
# first load, including any one-time migration.  On #37547 a 28-cell one-shot run
# reported the branch 2-7% SLOWER while the same cells on a persistent cart showed it
# 43.8% FASTER.  Same binaries, same config.  Reporting one number would have been
# reporting the opposite of the truth, so this reports both:
#
#   cold  a fresh cart every iteration -- session load plus first draw
#   warm  the same cart re-rendered by hgsid -- what a user clicking around pays
#
# Exit status: 0 if every cell matched on tracks and pixels, 1 otherwise.

set -u
export PATH=/usr/bin:/bin:/usr/local/bin:$PATH

HERE=$(cd "$(dirname "$0")" && pwd)

A_DIR=""; B_DIR=""
A_LABEL=""; B_LABEL=""
SCENARIOS="$HERE/scenarios.tsv"
OUT=""
ITERS=5
WARMUP=1
PROD_CONF=${PERF_PROD_CONF:-/usr/local/apache/cgi-bin/hg.conf}
APACHE=${PERF_APACHE:-/usr/local/apache}
# Where the committed session fixtures are copied so hgTracks can fetch them.  They
# are copied from this checkout on every run, so what is served is provably what is
# committed -- the fixture cannot rot independently of the test that names it.
FIX_DIR=${PERF_FIXTURE_DIR:-$HOME/public_html/perfNightlyFixtures}
FIX_URL=${PERF_FIXTURE_URL:-https://hgwdev.gi.ucsc.edu/~$USER/perfNightlyFixtures}
KEEP_PNG=0

usage() {
    sed -n '2,/^# Exit status/p' "$0" | sed 's/^# \{0,1\}//'
    echo
    echo "usage: compare.sh --a DIR --b DIR [options]"
    echo "  --a DIR --b DIR        directories holding hgTracks and hgRenderTracks"
    echo "  --a-label S --b-label S   names for the report (default: basename)"
    echo "  --scenarios FILE       default: $HERE/scenarios.tsv"
    echo "  --iters N              timed iterations per cell per side (default $ITERS)"
    echo "  --warmup N             discarded iterations (default $WARMUP)"
    echo "  --out DIR              work directory (default: a new dir under /data/tmp)"
    echo "  --keep-png             keep every rendered PNG, not just the ones that differ"
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --a) A_DIR=$2; shift 2;;
        --b) B_DIR=$2; shift 2;;
        --a-label) A_LABEL=$2; shift 2;;
        --b-label) B_LABEL=$2; shift 2;;
        --scenarios) SCENARIOS=$2; shift 2;;
        --iters) ITERS=$2; shift 2;;
        --warmup) WARMUP=$2; shift 2;;
        --out) OUT=$2; shift 2;;
        --keep-png) KEEP_PNG=1; shift;;
        -h|--help) usage;;
        *) echo "unknown argument: $1" >&2; usage;;
    esac
done

[ -n "$A_DIR" ] && [ -n "$B_DIR" ] || usage
A_DIR=$(cd "$A_DIR" && pwd) || { echo "no such directory: $A_DIR" >&2; exit 1; }
B_DIR=$(cd "$B_DIR" && pwd) || { echo "no such directory: $B_DIR" >&2; exit 1; }
[ -n "$A_LABEL" ] || A_LABEL=$(basename "$A_DIR")
[ -n "$B_LABEL" ] || B_LABEL=$(basename "$B_DIR")

for d in "$A_DIR" "$B_DIR"; do
    for b in hgTracks hgRenderTracks; do
        [ -x "$d/$b" ] || { echo "missing $d/$b -- build it with: make -C src/hg/hgTracks compile" >&2; exit 1; }
    done
done
[ -r "$SCENARIOS" ] || { echo "no scenario file: $SCENARIOS" >&2; exit 1; }
[ -r "$PROD_CONF" ] || { echo "cannot read $PROD_CONF" >&2; exit 1; }

[ -n "$OUT" ] || OUT=/data/tmp/perfCompare.$(date +%Y-%m-%d_%H%M).$$
mkdir -p "$OUT" || exit 1

# ---------------------------------------------------------------- helpers

urlenc() { python3 -c 'import sys,urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$1"; }

# Median of the numbers on stdin, in milliseconds.  Median rather than mean because a
# single GC or IO spike on a shared machine moves a mean and does not move a median.
median() { sort -n | awk '{a[NR]=$1} END{ if(NR==0){print "NA"; exit} m=int((NR+1)/2); if(NR%2) print a[m]; else print int((a[m]+a[m+1])/2) }'; }

# One side's private world: a CWD it owns, the two symlinks the direct path needs,
# and an hg.conf that includes production's but redirects the trackDb cache.
setup_side() {
    local tag=$1 root="$OUT/side.$1"
    mkdir -p "$root/cgi" "$root/tdbCache"
    ln -sfn "$APACHE/trash"  "$root/trash"
    ln -sfn "$APACHE/htdocs" "$root/htdocs"
    { echo "include $PROD_CONF"; echo "cacheTrackDbDir=$root/tdbCache"; } > "$root/hg.conf"
    echo "$root"
}

# Run one hgTracks invocation.  Prints elapsed milliseconds on stdout; the page goes
# to $3 and stderr to $4.
run_tracks() {
    local root=$1 bin=$2 page=$3 err=$4 qs=$5 s e
    s=$(date +%s%N)
    ( cd "$root/cgi" && HGDB_CONF="$root/hg.conf" "$bin" "$qs" > "$page" 2> "$err" )
    e=$(date +%s%N)
    echo $(( (e - s) / 1000000 ))
}

# The query string that puts a cell's tracks on in a FRESH cart.  Three forms, set by
# the scenario file's load column:
#   -                  trackDb defaults
#   session:FILE       load the committed session file by URL.  This is the only form
#                      that reproduces a real session: a flat replay of the file's
#                      name/value pairs as a GET does NOT, because composite _sel
#                      state, superTrack show state and the default-visibility pass
#                      all live in hgSession's load path, not in the variables.
#                      Measured 2026-09-15: the flat replay drew 23 of the 71 tracks
#                      and invented four the session never had.
#   q:FRAGMENT         a literal query fragment, e.g. q:hgt.visAllFromCt=pack.  Used
#                      for GenArk, where every track name carries a hub_<hubStatusId>_
#                      prefix that is assigned per machine -- a frozen fixture naming
#                      them would not survive a hubStatus change or move to another
#                      machine, and would silently address the wrong rows.
load_qs() {
    local load=$1
    case "$load" in
        -|"") echo "";;
        session:*) echo "&hgS_doLoadUrl=submit&hgS_loadUrlName=$(urlenc "$FIX_URL/${load#session:}")";;
        q:*) echo "&${load#q:}";;
        *) echo "unrecognised load form: $load" >&2; return 1;;
    esac
}

# The tracks hgTracks says it drew, sorted, one per line.
tracklog() {
    grep '^trackLog [0-9]' "$1" 2>/dev/null \
      | sed 's/^trackLog [0-9]* [^ ]* [^ ]* //' | tr ',' '\n' | grep ':' | sort
}

# ---------------------------------------------------------------- fixtures

if ls "$HERE"/sessions/*.session >/dev/null 2>&1; then
    mkdir -p "$FIX_DIR" && cp -f "$HERE"/sessions/*.session "$FIX_DIR"/ && chmod 644 "$FIX_DIR"/*.session
    if [ $? -ne 0 ]; then echo "could not publish fixtures to $FIX_DIR" >&2; exit 1; fi
fi

A_ROOT=$(setup_side a); B_ROOT=$(setup_side b)

# ---------------------------------------------------------------- run

TSV="$OUT/results.tsv"
printf 'cell\tmetric\t%s_ms\t%s_ms\tratio\n' "$A_LABEL" "$B_LABEL" > "$TSV"
REPORT="$OUT/report.txt"
fail=0
ncells=0

{
echo "hgTracks start-up and pixel comparison"
echo "  when:  $(date)"
echo "  host:  $(hostname)"
echo "  A:     $A_LABEL"
echo "         $A_DIR/hgTracks"
echo "         built $(date -r "$A_DIR/hgTracks" '+%Y-%m-%d %H:%M')  sha1 $(sha1sum "$A_DIR/hgTracks" | cut -c1-12)"
echo "  B:     $B_LABEL"
echo "         $B_DIR/hgTracks"
echo "         built $(date -r "$B_DIR/hgTracks" '+%Y-%m-%d %H:%M') sha1 $(sha1sum "$B_DIR/hgTracks" | cut -c1-12)"
echo "  iters: $ITERS timed, $WARMUP discarded, interleaved A/B"
echo "  conf:  $PROD_CONF, with a private cacheTrackDbDir per side"
echo
} > "$REPORT"

while IFS=$'\t' read -r name db load position pix; do
    case "$name" in ''|\#*) continue;; esac
    ncells=$((ncells + 1))
    lqs=$(load_qs "$load") || { fail=1; continue; }
    base="db=$db&position=$position&pix=$pix"

    declare -A SID PAGE ERRF
    # Warm each side on this cell before timing it.  This populates that side's own
    # trackDb cache and the shared udc cache, so a build with a new TRACKDB_VERSION
    # is not charged for a cost every later request avoids.
    for side in a b; do
        eval "root=\$${side^^}_ROOT dir=\$${side^^}_DIR"
        for w in $(seq 1 "$WARMUP"); do
            run_tracks "$root" "$dir/hgTracks" "$OUT/.warm.$side.html" "$OUT/.warm.$side.err" "$base$lqs" >/dev/null
        done
    done

    # cold: a fresh cart every iteration.  Interleaved so machine drift hits both.
    : > "$OUT/.cold.a"; : > "$OUT/.cold.b"
    for i in $(seq 1 "$ITERS"); do
        # Alternate which side goes first.  Running A first every time lets B ride on
        # the caches A has just warmed: measured 2026-09-15 as a systematic 4.5% in
        # B's favour with the SAME binary on both sides.  Alternating cancels it.
        if [ $(( i % 2 )) = 1 ]; then order="a b"; else order="b a"; fi
        for side in $order; do
            eval "root=\$${side^^}_ROOT dir=\$${side^^}_DIR"
            run_tracks "$root" "$dir/hgTracks" "$OUT/.cold.$side.html" "$OUT/.cold.$side.err" "$base$lqs" >> "$OUT/.cold.$side"
        done
    done

    # Establish one persistent cart per side, then settle it, so any one-time
    # migration is paid before the warm numbers start.
    ok=1
    for side in a b; do
        eval "root=\$${side^^}_ROOT dir=\$${side^^}_DIR"
        run_tracks "$root" "$dir/hgTracks" "$OUT/$name.$side.load.html" "$OUT/$name.$side.load.err" "$base$lqs" >/dev/null
        sid=$(grep -o 'hgsid=[0-9A-Za-z_]*' "$OUT/$name.$side.load.html" | head -1 | cut -d= -f2)
        if [ -z "$sid" ]; then
            echo "$name: $side produced no hgsid -- the cell did not render" >> "$REPORT"
            ok=0; continue
        fi
        SID[$side]=$sid
        run_tracks "$root" "$dir/hgTracks" "$OUT/$name.$side.html" "$OUT/$name.$side.err" \
                   "hgsid=$sid&$base" >/dev/null
        PAGE[$side]="$OUT/$name.$side.html"; ERRF[$side]="$OUT/$name.$side.err"
    done
    if [ $ok = 0 ]; then fail=1; continue; fi

    # warm: the same cart, re-rendered by hgsid.
    : > "$OUT/.warmt.a"; : > "$OUT/.warmt.b"
    for i in $(seq 1 "$ITERS"); do
        if [ $(( i % 2 )) = 1 ]; then order="a b"; else order="b a"; fi
        for side in $order; do
            eval "root=\$${side^^}_ROOT dir=\$${side^^}_DIR"
            run_tracks "$root" "$dir/hgTracks" "$OUT/.warmt.$side.html" "$OUT/.warmt.$side.err" \
                       "hgsid=${SID[$side]}&$base" >> "$OUT/.warmt.$side"
        done
    done

    # Did both sides draw the same tracks?  This is the check that catches a cart
    # format the other build cannot read, which otherwise looks like a speedup.
    tracklog "${ERRF[a]}" > "$OUT/$name.a.tracks"
    tracklog "${ERRF[b]}" > "$OUT/$name.b.tracks"
    # Two counts, because they are two different facts and a bug can move either.
    # trackLog names every track hgTracks LOADED, including the subtracks of a hidden
    # composite at their own trackDb visibility -- a default hg38 view logs 563 of
    # them.  A hidden track is not logged at all, so there are no :0 entries to
    # filter.  The drawn-row count comes from the page and is the visible truth.
    na=$(wc -l < "$OUT/$name.a.tracks"); nb=$(wc -l < "$OUT/$name.b.tracks")
    ra=$(grep -c "id='tr_" "${PAGE[a]}"); rb=$(grep -c "id='tr_" "${PAGE[b]}")
    if [ "$na" = 0 ] || [ "$nb" = 0 ]; then
        tracks_verdict="NOTHING LOADED ($A_LABEL=$na $B_LABEL=$nb)"; fail=1
    elif [ "$ra" != "$rb" ]; then
        tracks_verdict="ROW COUNT DIFFERS ($A_LABEL=$ra $B_LABEL=$rb rows)"; fail=1
        diff "$OUT/$name.a.tracks" "$OUT/$name.b.tracks" > "$OUT/$name.tracks.diff"
    elif diff -q "$OUT/$name.a.tracks" "$OUT/$name.b.tracks" >/dev/null; then
        tracks_verdict="same ($ra rows drawn, $na tracks loaded)"
        rm -f "$OUT/$name.a.tracks" "$OUT/$name.b.tracks"
    else
        tracks_verdict="TRACK SET DIFFERS ($A_LABEL=$na $B_LABEL=$nb loaded)"; fail=1
        diff "$OUT/$name.a.tracks" "$OUT/$name.b.tracks" > "$OUT/$name.tracks.diff"
    fi

    # Pixels.  hgRenderTracks returns the composed PNG on stdout, so there is no HTML
    # to scrape and no trash directory to find -- but run directly it emits its CGI
    # headers first, which have to come off before the bytes are a PNG.
    for side in a b; do
        eval "root=\$${side^^}_ROOT dir=\$${side^^}_DIR"
        ( cd "$root/cgi" && HGDB_CONF="$root/hg.conf" \
          "$dir/hgRenderTracks" "hgsid=${SID[$side]}&$base" 2>/dev/null ) \
          | sed '1,/^\r\{0,1\}$/d' > "$OUT/$name.$side.png"
    done
    if [ ! -s "$OUT/$name.a.png" ] || [ ! -s "$OUT/$name.b.png" ]; then
        pix_verdict="NO IMAGE"; fail=1
    else
        ae=$(compare -metric AE "$OUT/$name.a.png" "$OUT/$name.b.png" "$OUT/$name.diff.png" 2>&1)
        ae=${ae%% *}
        case "$ae" in
            0) pix_verdict="identical"
               rm -f "$OUT/$name.diff.png"
               [ "$KEEP_PNG" = 1 ] || rm -f "$OUT/$name.a.png" "$OUT/$name.b.png";;
            ''|*[!0-9]*) pix_verdict="DIMENSIONS DIFFER ($ae)"; fail=1;;
            *) pix_verdict="$ae PIXELS DIFFER"; fail=1;;
        esac
    fi

    ca=$(median < "$OUT/.cold.a"); cb=$(median < "$OUT/.cold.b")
    wa=$(median < "$OUT/.warmt.a"); wb=$(median < "$OUT/.warmt.b")
    rc=$(awk -v a="$ca" -v b="$cb" 'BEGIN{ if(a>0) printf "%.3f", b/a; else print "NA" }')
    rw=$(awk -v a="$wa" -v b="$wb" 'BEGIN{ if(a>0) printf "%.3f", b/a; else print "NA" }')
    printf '%s\tcold\t%s\t%s\t%s\n' "$name" "$ca" "$cb" "$rc" >> "$TSV"
    printf '%s\twarm\t%s\t%s\t%s\n' "$name" "$wa" "$wb" "$rw" >> "$TSV"

    {
    printf '%-22s %s\n' "$name" "$db $position"
    printf '   cold   %7s ms   %7s ms   x%-6s\n' "$ca" "$cb" "$rc"
    printf '   warm   %7s ms   %7s ms   x%-6s\n' "$wa" "$wb" "$rw"
    printf '   tracks %s\n' "$tracks_verdict"
    printf '   pixels %s\n' "$pix_verdict"
    echo
    } >> "$REPORT"

    unset SID PAGE ERRF
done < "$SCENARIOS"

# The overall figure is the geometric mean of the per-cell ratios, which is the right
# average for ratios; an arithmetic mean of them is biased by whichever cell is slowest.
{
echo "--- overall (geometric mean of per-cell ratios; >1 means $B_LABEL is slower) ---"
for m in cold warm; do
    awk -v m="$m" -F'\t' '$2==m && $5!="NA" { s += log($5); n++ }
        END { if(n) printf "   %-5s x%.3f  (%+.1f%%) over %d cells\n", m, exp(s/n), (exp(s/n)-1)*100, n }' "$TSV"
done
echo
if [ "$ncells" = 0 ]; then
    echo "NO CELLS RAN -- $SCENARIOS produced nothing.  That is a problem with this job."
    fail=1
elif [ $fail = 0 ]; then
    echo "$ncells cells: every cell drew the same tracks and the same pixels."
else
    echo "$ncells cells: SOMETHING DIFFERS -- read the per-cell lines above."
fi
echo
echo "work directory: $OUT"
} >> "$REPORT"

rm -f "$OUT"/.cold.* "$OUT"/.warm.* "$OUT"/.warmt.* "$OUT"/*.load.html "$OUT"/*.load.err
cat "$REPORT"
exit $fail

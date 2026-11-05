#!/bin/bash
# Nightly hgTracks start-up and pixel check against master, for cron.  refs #37547
#
# Same shape as the Docent regression nightly next door in hg/utils/docent/tests/regress:
# it runs out of a clone of its own, mails a report every night whether or not anything
# failed, keeps sixty days of logs, and always exits 0 so cron does not mail a second
# time on top of this one.  No mail means the cron has stopped, not that the browser is
# fine.
#
#     10 4 * * * ... nope -- that slot is the docent run.  Use:
#     30 4 * * * /hive/users/braney/perfNightly/kent/src/hg/utils/perfNightly/nightly.sh --update
#
# catalogNightly runs at 03:30 and the docent regression run at 04:10 for about seven
# minutes, and the build user's first pass is at 05:45.  04:30 is the quiet window.
#
# What it compares, and why it is not genome-test
# -----------------------------------------------
# A ROLLING REFERENCE BINARY: the hgTracks and hgRenderTracks from the last green night,
# kept in $ROOT/ref.  Every night this builds master fresh in its own clone and compares
# the two, on the same machine, against the same data, in the same minute.
#
# That is the only baseline that separates a code change from a data change.  ClinVar,
# GENCODE and the GenArk hubs all update underneath us, so a committed golden PNG or a
# day-old saved render goes red for reasons that have nothing to do with the tree.  Two
# binaries reading the same data at the same moment do not: a data change moves both
# renders equally and the pixels still match.  Only a code change moves them.
#
# It also does not read /usr/local/apache/cgi-bin.  That is rebuilt about seven times a
# day by the build user, but developers install into cgi-alpha by hand as well -- there
# are `braney, cgi-alpha` lines in its buildLog.txt -- so some nights it is not master.
#
# Green promotes today's binaries to be tomorrow's reference.  Red leaves the reference
# pinned, so the alert does not quietly disappear the next morning, and so the commit
# range named in the mail keeps covering the whole regression rather than just the last
# day of it.
#
#     nightly.sh --accept "reason"   promote the current build deliberately, after a
#                                    rendering change that was meant.  Writes one line
#                                    to accepted.log, which lives outside the checkout
#                                    because --update does `reset --hard`.
#
# Two alarms, because they catch different things
# -----------------------------------------------
#   today vs reference   a regression that arrived in one day's commits, and the mail
#                        names that commit range.
#   today vs 14 days     slow creep.  Three percent a day never trips the day ratio and
#                        is sixty percent in a month.  Read out of history.csv, which
#                        also lives outside the checkout and is never pruned.

set -u
export PATH=/usr/bin:/bin:/usr/local/bin:$PATH

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=${PERF_NIGHTLY_ROOT:-/hive/users/braney/perfNightly}
TO=${PERF_NIGHTLY_TO:-braney@ucsc.edu}
LOGDIR=$ROOT/logs
REF=$ROOT/ref
HISTORY=$ROOT/history.csv
ACCEPTED=$ROOT/accepted.log
ITERS=${PERF_NIGHTLY_ITERS:-5}
# How far today's absolute number may sit above the trailing median before it is called
# out.  hgwdev is shared, so this is deliberately loose: a self-comparison of the same
# binary measured +/-1.3% at four iterations, and a real morning has other jobs on it.
CREEP_PCT=${PERF_NIGHTLY_CREEP_PCT:-15}
CREEP_DAYS=${PERF_NIGHTLY_CREEP_DAYS:-14}
STAMP=$(date +%Y-%m-%d_%H%M)
SRC=$(cd "$HERE/../../.." && pwd)     # the src/ of this checkout

mkdir -p "$LOGDIR" "$REF"
OUT="$LOGDIR/$STAMP.txt"

# A job that could not start has to arrive looking like the others, or a night when
# nothing ran reads as a quiet night.
bail() {
{
    echo "hgTracks performance run, $(date)"
    echo "checkout: $HERE"
    echo
    echo "$@"
    echo
    echo "Nothing was measured.  This is a problem with this job, not with the browser."
} > "$OUT" 2>&1
mail -s "hgTracks perf: BROKEN $(date +%F)" "$TO" < "$OUT"
exit 0
}

# ---------------------------------------------------------------- --accept

if [ "${1:-}" = --accept ]; then
    shift
    reason="${*:-}"
    [ -n "$reason" ] || { echo "usage: nightly.sh --accept \"why this rendering change was intended\"" >&2; exit 1; }
    [ -x "$ROOT/today/hgTracks" ] || { echo "no $ROOT/today/hgTracks to accept -- run the nightly first" >&2; exit 1; }
    cp -f "$ROOT/today/hgTracks" "$ROOT/today/hgRenderTracks" "$REF/" || exit 1
    cp -f "$ROOT/today/STAMP" "$REF/STAMP" 2>/dev/null
    printf '%s  %-12s %s\n' "$(date +%F)" "$(cat "$REF/STAMP" 2>/dev/null | head -1)" "$reason" >> "$ACCEPTED"
    echo "reference promoted.  Recorded in $ACCEPTED:"
    tail -1 "$ACCEPTED"
    exit 0
fi

# ---------------------------------------------------------------- --update

if [ "${1:-}" = --update ] && [ -z "${PERF_NIGHTLY_UPDATED:-}" ]; then
    shift
    dirty=$(git -C "$HERE" status --porcelain --untracked-files=no 2>&1)
    [ -z "$dirty" ] || bail "--update will not reset $HERE: it has uncommitted changes.

$dirty"
    git -C "$HERE" fetch -q origin master 2>&1 || bail "--update could not fetch origin master into $HERE."
    ahead=$(git -C "$HERE" rev-list --oneline FETCH_HEAD..HEAD 2>&1)
    [ -z "$ahead" ] || bail "--update will not reset $HERE: it holds commits that are not on origin/master.

$ahead"
    git -C "$HERE" reset -q --hard FETCH_HEAD 2>&1 || bail "--update could not move $HERE to FETCH_HEAD."
    export PERF_NIGHTLY_UPDATED=1
    exec "$0" "$@"
fi

COMMIT=$(git -C "$HERE" rev-parse --short HEAD 2>/dev/null)

# ---------------------------------------------------------------- build

BUILDLOG=$LOGDIR/.build.$STAMP
{
    make -C "$SRC" -j8 libs && make -C "$SRC/hg/hgTracks" -j8 compile
} > "$BUILDLOG" 2>&1
if [ $? -ne 0 ] || [ ! -x "$SRC/hg/hgTracks/hgTracks" ]; then
    bail "The build of master failed at $COMMIT.  That is news about the tree, not about this job.

$(tail -40 "$BUILDLOG")"
fi

mkdir -p "$ROOT/today"
cp -f "$SRC/hg/hgTracks/hgTracks" "$SRC/hg/hgTracks/hgRenderTracks" "$ROOT/today/" || bail "could not stage today's binaries in $ROOT/today"
echo "$COMMIT $(date +%F_%H%M)" > "$ROOT/today/STAMP"
rm -f "$BUILDLOG"

# ---------------------------------------------------------------- first run seeds

if [ ! -x "$REF/hgTracks" ]; then
    cp -f "$ROOT/today/hgTracks" "$ROOT/today/hgRenderTracks" "$REF/"
    cp -f "$ROOT/today/STAMP" "$REF/STAMP"
{
    echo "hgTracks performance run, $(date)"
    echo "commit:   $COMMIT"
    echo
    echo "No reference binary existed, so this build became the reference."
    echo "The first real comparison happens tomorrow."
} > "$OUT"
    mail -s "hgTracks perf: reference seeded $(date +%F)" "$TO" < "$OUT"
    exit 0
fi

REF_STAMP=$(cat "$REF/STAMP" 2>/dev/null | head -1)
REF_COMMIT=${REF_STAMP%% *}
REF_AGE_D=$(( ( $(date +%s) - $(stat -c %Y "$REF/hgTracks") ) / 86400 ))

# ---------------------------------------------------------------- compare

WORK=$ROOT/work/$STAMP
rm -rf "$WORK"; mkdir -p "$WORK"
"$HERE/compare.sh" --a "$REF" --a-label "ref@$REF_COMMIT" \
                   --b "$ROOT/today" --b-label "master@$COMMIT" \
                   --scenarios "$HERE/scenarios.tsv" --iters "$ITERS" \
                   --out "$WORK" > "$WORK/stdout" 2>&1
cmp_rc=$?

# ---------------------------------------------------------------- creep

# Today's absolute numbers against the trailing median for the same cell and metric.
# The day-over-day ratio cannot see a drift that is small every day, and this cannot
# see a one-day jump that the reference moved with, so both are reported.
CREEP=$WORK/creep.txt
: > "$CREEP"
if [ -s "$HISTORY" ]; then
    cutoff=$(date -d "$CREEP_DAYS days ago" +%Y-%m-%d 2>/dev/null)
    python3 - "$HISTORY" "$WORK/results.tsv" "$cutoff" "$CREEP_PCT" > "$CREEP" <<'ENDPY'
import sys, csv, statistics
hist, today, cutoff, pct = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
past = {}
with open(hist) as f:
    for row in csv.reader(f):
        if len(row) != 5 or row[0] == 'date' or row[0] < cutoff: continue
        past.setdefault((row[2], row[3]), []).append(float(row[4]))
with open(today) as f:
    next(f, None)
    for line in f:
        p = line.rstrip('\n').split('\t')
        if len(p) != 5: continue
        cell, metric, _a, b = p[0], p[1], p[2], p[3]
        try: b = float(b)
        except ValueError: continue
        hs = past.get((cell, metric))
        if not hs or len(hs) < 3: continue
        med = statistics.median(hs)
        if med > 0 and (b - med) / med * 100 > pct:
            print("   %-22s %-5s %6.0f ms vs %.0f ms median of the last %d nights  (+%.0f%%)"
                  % (cell, metric, b, med, len(hs), (b - med) / med * 100))
ENDPY
fi

# ---------------------------------------------------------------- record

[ -s "$HISTORY" ] || echo "date,commit,cell,metric,ms" > "$HISTORY"
awk -F'\t' -v d="$(date +%F)" -v c="$COMMIT" 'NR>1 && NF==5 { print d "," c "," $1 "," $2 "," $4 }' \
    "$WORK/results.tsv" >> "$HISTORY" 2>/dev/null

# ---------------------------------------------------------------- report

{
echo "hgTracks start-up and pixel check, $(date)"
echo "checkout:  $HERE"
echo "commit:    $COMMIT"
echo "reference: $REF_COMMIT, built $REF_AGE_D day(s) ago"
if [ "$REF_AGE_D" -gt 7 ]; then
    echo
    echo "   The reference is over a week old.  That happens when a red night was never"
    echo "   cleared, and it means the commit range below is a week wide.  Either fix the"
    echo "   regression or run:  nightly.sh --accept \"why\""
fi
echo
if [ -n "$REF_COMMIT" ] && [ "$REF_COMMIT" != "$COMMIT" ]; then
    echo "--- commits between the reference and today ---"
    git -C "$HERE" log --oneline "$REF_COMMIT..$COMMIT" 2>/dev/null | head -40
    n=$(git -C "$HERE" rev-list --count "$REF_COMMIT..$COMMIT" 2>/dev/null)
    [ "${n:-0}" -gt 40 ] && echo "   ... and $(( n - 40 )) more"
    echo
fi
cat "$WORK/stdout"
if [ -s "$CREEP" ]; then
    echo
    echo "--- slower than the trailing $CREEP_DAYS-night median by more than $CREEP_PCT% ---"
    cat "$CREEP"
    echo
    echo "   This is the alarm the day-over-day ratio cannot raise.  A cell here has"
    echo "   drifted, not jumped."
fi
echo
echo "full log kept at $OUT"
} > "$OUT" 2>&1

# ---------------------------------------------------------------- promote or pin

if [ $cmp_rc = 0 ]; then
    cp -f "$ROOT/today/hgTracks" "$ROOT/today/hgRenderTracks" "$REF/"
    cp -f "$ROOT/today/STAMP" "$REF/STAMP"
    echo "reference promoted to $COMMIT." >> "$OUT"
    if [ -s "$CREEP" ]; then state="pass, but a cell has drifted"; else state=pass; fi
else
    echo "reference left pinned at $REF_COMMIT, because something differed." >> "$OUT"
    state=FAIL
fi

mail -s "hgTracks perf: $state $(date +%F)" "$TO" < "$OUT"

find "$LOGDIR" -name '*.txt' -mtime +60 -delete 2>/dev/null
# Keep a fortnight of work directories: the PNGs of a red night are what someone opens.
find "$ROOT/work" -maxdepth 1 -mtime +14 -exec rm -rf {} + 2>/dev/null

exit 0

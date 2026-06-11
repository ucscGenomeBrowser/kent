#!/bin/bash
#
# autoBuild.sh - Fully automated CGI build process
#
# Runs the appropriate build phase (preview1, preview2, final build,
# or wrap-up) based on the Google Calendar build schedule.
#
# Exits non-zero (and loudly) at the first sign of anything wrong.
# Designed to be run from cron or manually with no human interaction.
#
# Usage:
#   autoBuild.sh              # auto-detect phase from schedule
#   autoBuild.sh preview1     # force a specific phase
#   autoBuild.sh preview2
#   autoBuild.sh final
#   autoBuild.sh wrapup
#   autoBuild.sh --dry-run [phase]   # show what would happen
#
# BUILD PATCHES (not a phase here yet - done manually via the weeklybld scripts):
#   A build patch cherry-picks approved commit(s) onto v${NN}_branch after the
#   weekly build, then: tagBeta.csh real -> tag v${NN}_branch.<next> -> update the
#   Redmine ticket BEFORE any long artifact build.
#   If the patch lands AFTER the release has gone out to the RR, the already-public
#   artifacts must be regenerated too: rebuild the source zip (doZip.csh) and the
#   docker release images (the docker build/push/manifest steps below). A pre-RR
#   patch skips those. WAIT for the patched CGIs to reach hgdownload before the
#   docker release build: the amd64 image rsyncs prebuilt CGIs from the RR/production
#   cgi-bin module on hgdownload (browserSetup.sh -b is batch mode, not "beta"; the
#   actual source is hgdownload.soe.ucsc.edu::cgi-bin). That module is populated by
#   the external RR/production push (qateam), NOT by these weeklybld scripts, so it
#   only reflects the patch once that push has propagated. Only arm64 compiles from
#   the beta branch source, so it gets the patch immediately; amd64 built too early
#   bakes in stale, unpatched binaries. wait_for_hgdownload_cgis() below polls for it.
#
set -eEu -o pipefail

##############################################################################
# Configuration
##############################################################################
SCRIPT_NAME="$(basename "$0")"
WEEKLYBLD="/hive/groups/browser/newBuild/kent/src/utils/qa/weeklybld"
BUILDENV="$WEEKLYBLD/buildEnv.csh"
LOGDIR="$WEEKLYBLD/logs"
LOCKFILE="/tmp/autoBuild.lock"
GCAL_ICAL_URL="https://calendar.google.com/calendar/ical/ucsc.edu_vaaiq62mh73n78jonljfrnoof4%40group.calendar.google.com/public/basic.ics"
DRY_RUN=false
FORCED_PHASE=""

# wait_for_hgdownload_cgis() tunables (overridable from the environment):
#   the rsync module + probe CGI we watch, how often we poll, and how long we wait
#   before giving up. 3600s x 24 = poll hourly for up to ~24h.
HGDOWNLOAD_CGI_RSYNC="${HGDOWNLOAD_CGI_RSYNC:-hgdownload.soe.ucsc.edu::cgi-bin}"
HGDOWNLOAD_CGI_PROBE="${HGDOWNLOAD_CGI_PROBE:-hgTracks}"
HGDOWNLOAD_POLL_INTERVAL="${HGDOWNLOAD_POLL_INTERVAL:-3600}"
HGDOWNLOAD_POLL_MAX="${HGDOWNLOAD_POLL_MAX:-24}"
# Local freshly-built reference CGI; its mtime is the "not older than" bar the
# hgdownload copy must clear (i.e. hgdownload must carry a build at least this new).
HGDOWNLOAD_REF_CGI="${HGDOWNLOAD_REF_CGI:-/usr/local/apache/cgi-bin/hgTracks}"

##############################################################################
# Helpers
##############################################################################

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$SCRIPT_NAME] $*"
}

die() {
    log "FATAL: $*" >&2
    log "Build ABORTED." >&2
    if ! $DRY_RUN; then
        log "Sending alert email." >&2
        local subject="AUTOBUILD FAILED: $*"
        echo "$subject" | mail -s "$subject" "${BUILDMEISTEREMAIL:-braney@ucsc.edu}" 2>/dev/null || true
    fi
    exit 1
}

# Run a command, die if it fails. Log the command.
run() {
    log "RUN: $*"
    if $DRY_RUN; then
        log "(dry-run, skipped)"
        return 0
    fi
    "$@"
}

# Run a tcsh command in the WEEKLYBLD directory with buildEnv sourced.
# This is the core mechanism: all the existing .csh scripts expect tcsh
# with buildEnv.csh sourced and $WEEKLYBLD as cwd.
run_tcsh() {
    local cmd="$1"
    log "RUN(tcsh): $cmd"
    if $DRY_RUN; then
        log "(dry-run, skipped)"
        return 0
    fi
    /bin/tcsh -c "source $BUILDENV && cd $WEEKLYBLD && $cmd"
}

# Verify that a file exists and was modified recently (within N minutes).
check_file_fresh() {
    local file="$1"
    local max_age_min="${2:-120}"
    if [[ ! -f "$file" ]]; then
        die "Expected file does not exist: $file"
    fi
    local age_min
    age_min=$(( ($(date +%s) - $(stat -c %Y "$file")) / 60 ))
    if (( age_min > max_age_min )); then
        die "File $file is $age_min minutes old (expected < $max_age_min min)"
    fi
    log "OK: $file exists and is ${age_min}m old"
}

# Register QEMU binfmt_misc handlers so `docker build --platform linux/arm64`
# works on an amd64 host. Handlers are kernel state and are cleared on reboot,
# so we re-register on every build. The container is idempotent and fast (~1s)
# when handlers are already registered.
ensure_binfmt() {
    log "Registering QEMU binfmt handlers for cross-arch docker builds..."
    if run docker run --privileged --rm tonistiigi/binfmt --install all >/dev/null 2>&1; then
        log "OK: binfmt handlers registered"
    else
        log "WARNING: binfmt registration failed; arm64 docker builds may fail"
    fi
}

# Echo the epoch mtime of the probe CGI on hgdownload (via rsync --list-only), or
# empty on failure. rsync --list-only prints e.g. "-rwxr-xr-x 12345 2026/06/11 09:00:00 hgTracks".
hgdownload_cgi_mtime() {
    local line
    line=$(rsync --list-only --no-motd "${HGDOWNLOAD_CGI_RSYNC}/${HGDOWNLOAD_CGI_PROBE}" 2>/dev/null) || return 0
    # fields 3 (YYYY/MM/DD) and 4 (HH:MM:SS) -> epoch
    local d t
    d=$(echo "$line" | awk '{print $3}')
    t=$(echo "$line" | awk '{print $4}')
    [[ -n "$d" && -n "$t" ]] || return 0
    date -d "${d//\//-} $t" +%s 2>/dev/null || true
}

# Block until the patched CGIs have propagated to the RR/production cgi-bin module
# on hgdownload (the source the amd64 docker image rsyncs from). The bar is the
# mtime of the locally-built reference CGI (HGDOWNLOAD_REF_CGI): we wait until the
# hgdownload copy is at least that new, meaning hgdownload now carries a build no
# older than the one we just produced. Polls hourly (HGDOWNLOAD_POLL_INTERVAL) for
# up to HGDOWNLOAD_POLL_MAX tries, then dies loudly so a human can check the RR push.
# If hgdownload is already current (push preceded us) it returns immediately, so this
# is safe to call unconditionally before the docker release build.
wait_for_hgdownload_cgis() {
    local ref_mtime
    if [[ -f "$HGDOWNLOAD_REF_CGI" ]]; then
        ref_mtime=$(stat -c %Y "$HGDOWNLOAD_REF_CGI")
    else
        log "WARNING: reference CGI $HGDOWNLOAD_REF_CGI not found; using current time as the bar."
        ref_mtime=$(date +%s)
    fi
    log "Waiting for hgdownload CGIs to reach the patched build (bar: $(date -d @"$ref_mtime" '+%Y-%m-%d %H:%M:%S'), probe: ${HGDOWNLOAD_CGI_RSYNC}/${HGDOWNLOAD_CGI_PROBE})."
    if $DRY_RUN; then
        log "[dry-run] would poll every $((HGDOWNLOAD_POLL_INTERVAL/60)) min (up to $HGDOWNLOAD_POLL_MAX tries) until hgdownload mtime >= bar."
        return 0
    fi
    local attempt=0 remote
    while true; do
        attempt=$((attempt + 1))
        remote=$(hgdownload_cgi_mtime)
        if [[ -n "$remote" && "$remote" -ge "$ref_mtime" ]]; then
            log "hgdownload CGIs are current ($(date -d @"$remote" '+%Y-%m-%d %H:%M:%S') >= bar) after $attempt poll(s). Proceeding with docker release build."
            return 0
        fi
        if (( attempt >= HGDOWNLOAD_POLL_MAX )); then
            die "Gave up after $attempt polls (~$((attempt*HGDOWNLOAD_POLL_INTERVAL/3600))h): ${HGDOWNLOAD_CGI_PROBE} on hgdownload is still older than the patched build. Has the RR/production push run?"
        fi
        local shown="${remote:-unreadable}"
        [[ -n "$remote" ]] && shown="$(date -d @"$remote" '+%Y-%m-%d %H:%M:%S')"
        log "Poll $attempt/$HGDOWNLOAD_POLL_MAX: hgdownload ${HGDOWNLOAD_CGI_PROBE} mtime $shown < bar; sleeping $((HGDOWNLOAD_POLL_INTERVAL/60)) min."
        sleep "$HGDOWNLOAD_POLL_INTERVAL"
    done
}

# Check that we are on the master branch (in the WEEKLYBLD git repo).
ensure_master_branch() {
    local branch
    branch=$(cd "$WEEKLYBLD" && git branch --show-current)
    if [[ "$branch" != "master" ]]; then
        die "WEEKLYBLD git repo is on branch '$branch', expected 'master'"
    fi
    log "OK: on master branch"
}

# Pull latest and check for uncommitted changes.
ensure_clean_git() {
    cd "$WEEKLYBLD"
    local status_out
    status_out=$(git status --porcelain | grep -v '^??' || true)
    if [[ -n "$status_out" ]]; then
        die "Uncommitted changes in $WEEKLYBLD:\n$status_out"
    fi
    log "OK: git working tree clean"
    run git pull
    log "OK: git pulled"
}

# Acquire lockfile (prevent concurrent builds).
acquire_lock() {
    if [[ -f "$LOCKFILE" ]]; then
        local lock_pid
        lock_pid=$(cat "$LOCKFILE")
        if kill -0 "$lock_pid" 2>/dev/null; then
            die "Another autoBuild is running (PID $lock_pid, lockfile $LOCKFILE)"
        else
            log "WARNING: Stale lockfile from PID $lock_pid, removing."
            rm -f "$LOCKFILE"
        fi
    fi
    echo $$ > "$LOCKFILE"
    trap 'rm -f "$LOCKFILE"' EXIT
    log "Lock acquired (PID $$)"
}

# Read current buildEnv.csh values into bash variables.
read_buildenv() {
    eval "$(tcsh -c "source $BUILDENV && echo BRANCHNN=\$BRANCHNN && echo TODAY=\$TODAY && echo LASTWEEK=\$LASTWEEK && echo REVIEWDAY=\$REVIEWDAY && echo LASTREVIEWDAY=\$LASTREVIEWDAY && echo REVIEW2DAY=\$REVIEW2DAY && echo LASTREVIEW2DAY=\$LASTREVIEW2DAY && echo BUILDMEISTEREMAIL=\$BUILDMEISTEREMAIL && echo BUILDHOME=\$BUILDHOME")"
    export BRANCHNN TODAY LASTWEEK REVIEWDAY LASTREVIEWDAY REVIEW2DAY LASTREVIEW2DAY BUILDMEISTEREMAIL BUILDHOME
    log "Current buildEnv: BRANCHNN=$BRANCHNN TODAY=$TODAY REVIEWDAY=$REVIEWDAY REVIEW2DAY=$REVIEW2DAY"
}

##############################################################################
# Checkpoint / resume framework
#
# Each phase is a sequence of named steps. Completed steps are appended to a
# per-phase, per-version state file under $LOGDIR. On restart, completed steps
# are skipped and the phase resumes at the first incomplete step, so a re-run
# after a mid-build failure runs the non-idempotent steps (version bump,
# CGI-version commit, branch tag, team emails) exactly once. As a second safety
# net, the buildEnv bump self-detects an already-applied bump by date, so it can
# never double-increment even if the state file is lost.
#
# The state file is NOT removed on success: it doubles as a per-build record,
# and re-running a fully-completed phase is then a harmless all-steps-skipped
# no-op rather than a dangerous fresh start.
##############################################################################

STATE_FILE=""

# Version (NN) the current phase produces. preview1/preview2 produce BRANCHNN+1
# (BRANCHNN is not bumped until the final build), final/wrapup produce BRANCHNN.
# Each do_* sets this so the success email reports the right version.
PHASE_VER=""

state_init() {
    # $1 = phase name, $2 = version (NN) this phase produces
    mkdir -p "$LOGDIR"
    STATE_FILE="$LOGDIR/.autobuild.state.v${2}.${1}"
    if [[ -f "$STATE_FILE" ]]; then
        local done_steps
        done_steps=$(tr '\n' ' ' < "$STATE_FILE")
        log "Resuming phase '$1' for v$2 (state file: $STATE_FILE)"
        log "Already-completed steps: ${done_steps:-(none)}"
    else
        $DRY_RUN || : > "$STATE_FILE"
        log "Starting phase '$1' for v$2 (state file: $STATE_FILE)"
    fi
}

step_is_done() { grep -qxF "$1" "$STATE_FILE" 2>/dev/null; }

# step <id> <function> [args...]
# Run the step once. If already recorded as done, skip it. On success, record
# it; on failure, die (the state file keeps every step completed before this
# one, so re-running resumes here). NOTE: because the step body runs in an `if`
# condition, `set -e` does not abort inside it -- each step function below is
# written so its RETURN VALUE reflects success (critical command last, or
# `|| return 1`, or an explicit die on a checked failure).
step() {
    local id="$1"; shift
    if step_is_done "$id"; then
        log "SKIP (already done): $id"
        return 0
    fi
    log ">>> STEP START: $id"
    if "$@"; then
        $DRY_RUN || echo "$id" >> "$STATE_FILE"
        log "<<< STEP DONE:  $id"
    else
        die "step '$id' failed. Fix the problem and re-run autoBuild.sh to resume from this step."
    fi
}

##############################################################################
# Determine which phase to run from Google Calendar
##############################################################################

detect_phase() {
    local ds_dash
    ds_dash=$(date "+%F")           # YYYY-MM-DD for logs
    local ds_ical
    ds_ical=$(date "+%Y%m%d")       # YYYYMMDD for iCal DTSTART matching

    # Primary source: Google Calendar iCal feed
    local entry=""
    log "Fetching build schedule from Google Calendar..." >&2
    local ical_data
    ical_data=$(curl -sS --max-time 30 "$GCAL_ICAL_URL" 2>/dev/null) || true

    if [[ -n "$ical_data" ]]; then
        # Parse iCal: find VEVENT whose DTSTART matches today, extract SUMMARY
        entry=$(echo "$ical_data" | awk -v date="$ds_ical" '
            /^BEGIN:VEVENT/ { in_event=1; summary=""; dtstart="" }
            /^END:VEVENT/ {
                if (in_event && dtstart == date && summary != "") print summary
                in_event=0
            }
            in_event && /^DTSTART/ {
                gsub(/.*:/, ""); gsub(/\r/, ""); dtstart=$0
            }
            in_event && /^SUMMARY/ {
                gsub(/^SUMMARY:/, ""); gsub(/\r/, ""); summary=$0
            }
        ' | head -1)

        if [[ -n "$entry" ]]; then
            log "Google Calendar entry for $ds_dash: $entry" >&2
        else
            log "WARNING: No Google Calendar entry for today ($ds_dash)" >&2
        fi
    else
        log "WARNING: Could not fetch Google Calendar iCal feed" >&2
    fi

    if [[ -z "$entry" ]]; then
        die "No entry for today ($ds_dash) in Google Calendar. Is the calendar up to date?"
    fi

    log "Schedule entry for $ds_dash: $entry" >&2

    # Match digit or Roman-numeral forms ("Preview 1"/"Preview I", "Preview 2"/"Preview II").
    # Check II/2 before I/1 so "Preview II" does not fall through to preview1.
    if echo "$entry" | grep -qiE 'preview (2|ii)\b'; then
        echo "preview2"
    elif echo "$entry" | grep -qiE 'preview (1|i)\b'; then
        echo "preview1"
    elif echo "$entry" | grep -qi "final build"; then
        echo "final"
    else
        die "Unrecognized schedule entry: '$entry'"
    fi
}

##############################################################################
# Phase: Preview 1 (Day 1)
##############################################################################

# --- Preview 1 steps (decomposed from doNewReview.csh for resumability) ---

preview1_buildenv_bump() {
    read_buildenv
    if [[ "$REVIEWDAY" == "$(date +%F)" ]]; then
        log "buildEnv already bumped for today (REVIEWDAY=$REVIEWDAY) - skipping bump"
        return 0
    fi
    local NEXTNN=$((BRANCHNN + 1))
    local new_REVIEWDAY; new_REVIEWDAY=$(date "+%F")
    local new_LASTREVIEWDAY="$REVIEWDAY"
    log "Updating buildEnv.csh: LASTREVIEWDAY=$new_LASTREVIEWDAY -> REVIEWDAY=$new_REVIEWDAY (v${NEXTNN} preview)"
    if [[ "$new_REVIEWDAY" < "$new_LASTREVIEWDAY" ]]; then
        die "Date sanity check failed: new REVIEWDAY ($new_REVIEWDAY) < new LASTREVIEWDAY ($new_LASTREVIEWDAY)"
    fi
    if ! $DRY_RUN; then
        sed -i \
            -e "s|^setenv LASTREVIEWDAY .*|setenv LASTREVIEWDAY ${new_LASTREVIEWDAY}                     # v${BRANCHNN} preview|" \
            -e "s|^setenv REVIEWDAY .*|setenv REVIEWDAY ${new_REVIEWDAY}                     # v${NEXTNN} preview|" \
            "$BUILDENV"
        grep -q "setenv REVIEWDAY ${new_REVIEWDAY}" "$BUILDENV" || die "buildEnv.csh edit verification failed for REVIEWDAY"
        grep -q "setenv LASTREVIEWDAY ${new_LASTREVIEWDAY}" "$BUILDENV" || die "buildEnv.csh edit verification failed for LASTREVIEWDAY"
    fi
    run_tcsh "source $BUILDENV && env | egrep VIEWDAY" || return 1
    run_tcsh "cd $WEEKLYBLD && git add buildEnv.csh && git commit -m 'v${NEXTNN} preview1 (automated)' buildEnv.csh && git push" || return 1
    read_buildenv
    return 0
}

preview1_tag_preview() {
    run_tcsh "./tagPreview.csh real >& $LOGDIR/v$((BRANCHNN + 1)).tagPreview.log"
}

preview1_git_reports() {
    run_tcsh "./buildGitReports.csh review real >& $LOGDIR/v$((BRANCHNN + 1)).gitReports.review.log"
}

preview1_email_pairings() {
    local NEXTNN=$((BRANCHNN + 1))
    if $DRY_RUN; then
        log "(dry-run) would email 'Ready for pairings (day 2, v${NEXTNN} preview)' to $BUILDMEISTEREMAIL clayfischer jnavarr5"
        return 0
    fi
    echo "Ready for pairings, day 2, Git reports completed for v${NEXTNN} preview http://genecats.gi.ucsc.edu/git-reports/ (history at http://genecats.gi.ucsc.edu/git-reports-history/)." \
        | mail -s "Ready for pairings (day 2, v${NEXTNN} preview)." "$BUILDMEISTEREMAIL" clayfischer@ucsc.edu jnavarr5@ucsc.edu
}

do_preview1() {
    log "========== PHASE: PREVIEW 1 =========="
    read_buildenv
    # Preview produces v(BRANCHNN+1); BRANCHNN is unchanged by a preview bump.
    PHASE_VER=$((BRANCHNN + 1))
    state_init preview1 "$PHASE_VER"

    step buildenv-bump   preview1_buildenv_bump
    step tag-preview     preview1_tag_preview
    step git-reports     preview1_git_reports
    step email-pairings  preview1_email_pairings

    log "Preview 1 complete."
}

##############################################################################
# Phase: Preview 2 (Day 8)
##############################################################################

# --- Preview 2 steps (decomposed from doNewReview2.csh for resumability) ---

preview2_buildenv_bump() {
    read_buildenv
    if [[ "$REVIEW2DAY" == "$(date +%F)" ]]; then
        log "buildEnv already bumped for today (REVIEW2DAY=$REVIEW2DAY) - skipping bump"
        return 0
    fi
    local NEXTNN=$((BRANCHNN + 1))
    local new_REVIEW2DAY; new_REVIEW2DAY=$(date "+%F")
    local new_LASTREVIEW2DAY="$REVIEW2DAY"
    log "Updating buildEnv.csh: LASTREVIEW2DAY=$new_LASTREVIEW2DAY -> REVIEW2DAY=$new_REVIEW2DAY (v${NEXTNN} preview2)"
    if [[ "$new_REVIEW2DAY" < "$new_LASTREVIEW2DAY" ]]; then
        die "Date sanity check failed: new REVIEW2DAY ($new_REVIEW2DAY) < new LASTREVIEW2DAY ($new_LASTREVIEW2DAY)"
    fi
    if ! $DRY_RUN; then
        sed -i \
            -e "s|^setenv LASTREVIEW2DAY .*|setenv LASTREVIEW2DAY  ${new_LASTREVIEW2DAY}               # v${BRANCHNN} preview2|" \
            -e "s|^setenv REVIEW2DAY .*|setenv REVIEW2DAY  ${new_REVIEW2DAY}               # v${NEXTNN} preview2|" \
            "$BUILDENV"
        grep -q "setenv REVIEW2DAY  ${new_REVIEW2DAY}" "$BUILDENV" || die "buildEnv.csh edit verification failed for REVIEW2DAY"
    fi
    run_tcsh "source $BUILDENV && env | grep 2DAY" || return 1
    run_tcsh "cd $WEEKLYBLD && git add buildEnv.csh && git commit -m 'v${NEXTNN} preview2 (automated)' buildEnv.csh && git push" || return 1
    read_buildenv
    return 0
}

preview2_tag_preview2() {
    run_tcsh "./tagPreview2.csh real >& $LOGDIR/v$((BRANCHNN + 1)).tagPreview2.log"
}

preview2_git_reports() {
    run_tcsh "./buildGitReports.csh review2 real >& $LOGDIR/v$((BRANCHNN + 1)).gitReports.review2.log"
}

preview2_email_pairings() {
    local NEXTNN=$((BRANCHNN + 1))
    if $DRY_RUN; then
        log "(dry-run) would email 'Ready for pairings (day 9, v${NEXTNN} preview2)' to $BUILDMEISTEREMAIL clayfischer jnavarr5"
        return 0
    fi
    echo "Ready for pairings, day 9, Git reports completed for v${NEXTNN} preview2 http://genecats.gi.ucsc.edu/git-reports/ (history at http://genecats.gi.ucsc.edu/git-reports-history/)." \
        | mail -s "Ready for pairings (day 9, v${NEXTNN} preview2)." "$BUILDMEISTEREMAIL" clayfischer@ucsc.edu jnavarr5@ucsc.edu
}

preview2_tables_robot() {
    local NEXTNN=$((BRANCHNN + 1))
    log "Running preview2TablesTestRobot.csh (takes ~1h40m)..."
    run_tcsh "time ./preview2TablesTestRobot.csh >& $LOGDIR/v${NEXTNN}.preview2.hgTables.log"
}

do_preview2() {
    log "========== PHASE: PREVIEW 2 =========="
    read_buildenv
    PHASE_VER=$((BRANCHNN + 1))
    state_init preview2 "$PHASE_VER"

    step buildenv-bump   preview2_buildenv_bump
    step tag-preview2    preview2_tag_preview2
    step git-reports     preview2_git_reports
    step email-pairings  preview2_email_pairings
    step tables-robot    preview2_tables_robot

    log "Preview 2 complete."
}

##############################################################################
# Phase: Final Build (Day 15)
##############################################################################

# --- Final build steps (decomposed from doNewBranch.csh + the robots/docker
#     tail of the original do_final, so a mid-build failure resumes here) ---

final_checklogins() {
    log "Checking SSH logins to remote hosts..."
    run bash "$WEEKLYBLD/checkLogins.sh" 2>&1 | tee /tmp/autoBuild_checkLogins.log || true
    if grep -qi "failed" /tmp/autoBuild_checkLogins.log 2>/dev/null; then
        die "SSH login check failed. Fix SSH keys before proceeding. See /tmp/autoBuild_checkLogins.log"
    fi
    log "OK: All SSH logins successful"
}

final_buildenv_bump() {
    read_buildenv
    if [[ "$TODAY" == "$(date +%F)" ]]; then
        log "buildEnv already bumped for today (BRANCHNN=$BRANCHNN, TODAY=$TODAY) - skipping bump"
        return 0
    fi
    local new_BRANCHNN=$((BRANCHNN + 1))
    local new_TODAY; new_TODAY=$(date "+%F")
    local new_LASTWEEK="$TODAY"
    log "Updating buildEnv.csh: BRANCHNN=$BRANCHNN -> $new_BRANCHNN, LASTWEEK=$new_LASTWEEK, TODAY=$new_TODAY"
    if [[ "$new_TODAY" < "$new_LASTWEEK" ]]; then
        die "Date sanity check failed: new TODAY ($new_TODAY) < new LASTWEEK ($new_LASTWEEK)"
    fi
    if [[ $new_BRANCHNN -le $BRANCHNN ]]; then
        die "BRANCHNN sanity check failed: $new_BRANCHNN <= $BRANCHNN"
    fi
    if ! $DRY_RUN; then
        sed -i \
            -e "s|^setenv BRANCHNN .*|setenv BRANCHNN ${new_BRANCHNN}                    # increment for new build|" \
            -e "s|^setenv TODAY .*|setenv TODAY ${new_TODAY}                     # v${new_BRANCHNN} final|" \
            -e "s|^setenv LASTWEEK .*|setenv LASTWEEK ${new_LASTWEEK}                     # v${BRANCHNN} final|" \
            "$BUILDENV"
        grep -q "setenv BRANCHNN ${new_BRANCHNN}" "$BUILDENV" || die "BRANCHNN edit verification failed"
        grep -q "setenv TODAY ${new_TODAY}" "$BUILDENV" || die "TODAY edit verification failed"
        grep -q "setenv LASTWEEK ${new_LASTWEEK}" "$BUILDENV" || die "LASTWEEK edit verification failed"
    fi
    run_tcsh "source $BUILDENV && env | egrep 'DAY|NN|WEEK'" || return 1
    run_tcsh "cd $WEEKLYBLD && git add buildEnv.csh && git commit -m 'v${new_BRANCHNN} final build (automated)' buildEnv.csh && git push" || return 1
    read_buildenv
    log "After bump: BRANCHNN=$BRANCHNN"
    return 0
}

# updateCgiVersion.csh / tagNewBranch.csh are NOT idempotent on their own (a
# re-run would hit "nothing to commit" / "tag exists"), so the checkpoint must
# ensure each runs exactly once -- which it does.
final_cgi_version() {
    run_tcsh "./updateCgiVersion.csh real >& $LOGDIR/v${BRANCHNN}.updateCgiVersion.log"
}

final_tag_branch() {
    run_tcsh "./tagNewBranch.csh real >& $LOGDIR/v${BRANCHNN}.tagNewBranch.log"
}

final_git_reports() {
    if ! $DRY_RUN && [[ -e "$WEEKLYBLD/GitReports.ok" ]]; then
        rm -f "$WEEKLYBLD/GitReports.ok"
    fi
    run_tcsh "./buildGitReports.csh branch real >& doNewGit.log" || return 1
    if ! $DRY_RUN && [[ ! -e "$WEEKLYBLD/GitReports.ok" ]]; then
        die "git-reports completed but GitReports.ok not found. See doNewGit.log"
    fi
    return 0
}

final_cobranch() {
    run_tcsh "./coBranch.csh"
}

final_build_utils() {
    run_tcsh "./buildUtils.csh"
}

final_build_beta() {
    run_tcsh "./buildBeta.csh"
}

final_deploy_beta() {
    if $DRY_RUN; then
        log "(dry-run) would rsync cgi-bin-beta + htdocs-beta to qateam@hgwbeta"
        return 0
    fi
    rsync -rLptgoD -P --exclude=hg.conf --exclude=hg.conf.private \
        /usr/local/apache/cgi-bin-beta/ qateam@hgwbeta:/data/apache/cgi-bin/ && \
    rsync -a -P --exclude=hg.conf --exclude=hg.conf.private \
        /usr/local/apache/htdocs-beta/ qateam@hgwbeta:/data/apache/htdocs/ && \
    rsync -a -P --exclude=hg.conf --exclude=hg.conf.private --delete \
        /usr/local/apache/htdocs-beta/js/ qateam@hgwbeta:/data/apache/htdocs/js/ && \
    rsync -a -P --exclude=hg.conf --exclude=hg.conf.private --delete \
        /usr/local/apache/htdocs-beta/style/ qateam@hgwbeta:/data/apache/htdocs/style/
}

final_email_build_complete() {
    if $DRY_RUN; then
        log "(dry-run) would email 'v${BRANCHNN} Build complete on beta (day 16)' to $BUILDMEISTEREMAIL galt kent browser-qa"
        return 0
    fi
    echo "v${BRANCHNN} built successfully on beta (day 16)." \
        | mail -s "v${BRANCHNN} Build complete on beta (day 16)." \
        "$BUILDMEISTEREMAIL" galt@soe.ucsc.edu kent@soe.ucsc.edu browser-qa@soe.ucsc.edu
}

final_email_pairings() {
    if $DRY_RUN; then
        log "(dry-run) would email 'Ready for pairings (day 16, v${BRANCHNN} review)' to $BUILDMEISTEREMAIL clayfischer jnavarr5"
        return 0
    fi
    echo "Ready for pairings, day 16, Git reports completed for v${BRANCHNN} branch http://genecats.gi.ucsc.edu/git-reports/ (history at http://genecats.gi.ucsc.edu/git-reports-history/)." \
        | mail -s "Ready for pairings (day 16, v${BRANCHNN} review)." \
        "$BUILDMEISTEREMAIL" clayfischer@ucsc.edu jnavarr5@ucsc.edu
}

# Emails every v(BRANCHNN) committer a request for their code summaries. Kept a
# separate checkpoint so a resume never re-spams the whole committer list.
final_committer_emails() {
    local LASTNN=$((BRANCHNN - 1))
    run_tcsh "./sendLogEmail.pl $LASTNN $BRANCHNN"
}

final_robots() {
    if $DRY_RUN; then
        log "(dry-run) would launch doRobots.csh in background (6+ hours)"
        return 0
    fi
    local robotlog="$LOGDIR/v${BRANCHNN}.robots.log"
    nohup /bin/tcsh -c "source $BUILDENV && cd $WEEKLYBLD && ./doRobots.csh >& $robotlog" >/dev/null 2>&1 &
    log "doRobots.csh launched in background (PID $!). Log: $robotlog"
    return 0
}

# Build the beta image locally on hgwdev; do NOT push to Docker Hub. kent-beta
# runs from this image and is torn down again on do_wrapup. refs #37655.
# amd64 only: the container only runs on hgwdev (amd64) and is never pushed, so
# no arm64 build, no manifest, no binfmt. Now fatal-on-failure (was a warning):
# the checkpoint model makes a retry just a re-run that resumes at this step.
final_docker_beta() {
    local dockerdir="$BUILDHOME/v${BRANCHNN}_branch/kent/src/product/installer/docker"
    if [[ ! -d "$dockerdir" ]]; then
        die "Docker directory not found at $dockerdir"
    fi
    run_tcsh "cd $dockerdir && docker build --no-cache --platform linux/amd64 -t kent:beta . >& $LOGDIR/v${BRANCHNN}.docker-beta.log"
}

final_refresh_beta() {
    run "$WEEKLYBLD/refresh-instance.sh" beta
}

do_final() {
    log "========== PHASE: FINAL BUILD =========="
    read_buildenv
    # If TODAY already == today, the bump ran on a prior (failed) attempt and
    # BRANCHNN is already the new version; otherwise this run will bump to +1.
    local FINAL_VER
    if [[ "$TODAY" == "$(date +%F)" ]]; then
        FINAL_VER=$BRANCHNN
    else
        FINAL_VER=$((BRANCHNN + 1))
    fi
    PHASE_VER=$FINAL_VER
    state_init final "$FINAL_VER"

    step checklogins          final_checklogins
    step buildenv-bump        final_buildenv_bump
    step cgi-version          final_cgi_version
    step tag-branch           final_tag_branch
    step git-reports          final_git_reports
    step cobranch             final_cobranch
    step build-utils          final_build_utils
    step build-beta           final_build_beta
    step deploy-beta          final_deploy_beta
    step email-build-complete final_email_build_complete
    step email-pairings       final_email_pairings
    step committer-emails     final_committer_emails
    step robots               final_robots
    step docker-beta          final_docker_beta
    step refresh-beta         final_refresh_beta

    log "Final Build complete. Robots running in background."
    log "Next steps: QA tests on hgwbeta, then cherry-picks as needed, then push."
}

##############################################################################
# Generate markdown release notes from the versions page
##############################################################################

generate_release_markdown() {
    local ver="$1"
    local outdir="$WEEKLYBLD/markdownReleaseNotes"
    local outfile="$outdir/v${ver}_markdown.txt"
    local url="https://genecats.gi.ucsc.edu/builds/versions-all/v${ver}.html"

    mkdir -p "$outdir"

    log "Generating markdown release notes for v${ver}..."

    local html
    html=$(curl -s "$url") || {
        log "WARNING: Could not fetch $url - skipping markdown generation"
        return 0
    }

    if [[ -z "$html" ]]; then
        log "WARNING: Empty response from $url - skipping markdown generation"
        return 0
    fi

    # Extract code changes <li> items, strip HTML tags, redmine links, and author names
    echo "$html" \
        | sed -n '/<H2>Code changes:<\/H2>/,/<H2>/p' \
        | grep '<li>' \
        | sed 's|<li>||g; s|</li>||g' \
        | sed 's|<a [^>]*>[^<]*</a>||g' \
        | sed 's| ([^)]*)\. [A-Z][a-z]*$||' \
        | sed 's|\. [A-Z][a-z]*$||' \
        | sed 's|[[:space:]]*$||' \
        | sed 's|&lt;|<|g; s|&gt;|>|g; s|&amp;|\&|g; s|&quot;|"|g' \
        | sed 's|^|- |' \
        > "$outfile"

    log "Markdown release notes written to $outfile"
}

##############################################################################
# Phase: Wrap-up (Day 23, after push to RR)
##############################################################################

do_wrapup() {
    log "========== PHASE: WRAP-UP =========="
    read_buildenv
    PHASE_VER=$BRANCHNN

    log "Running wrap-up for v${BRANCHNN}"

    # Step 1: Build hgcentral SQL
    log "Building hgcentral SQL (dry run first)..."
    local hgclog="$LOGDIR/v${BRANCHNN}.buildHgCentralSql.log"
    run_tcsh "./buildHgCentralSql.csh >& $hgclog"
    log "hgcentral dry-run complete. Checking for differences..."

    if grep -q "No differences" "$hgclog" 2>/dev/null; then
        log "No hgcentral differences - skipping real hgcentral push."
    else
        log "hgcentral differences found. Running for real..."
        run_tcsh "./buildHgCentralSql.csh real >>& $hgclog"
        local rc=$?
        if [[ $rc -ne 0 ]]; then
            die "buildHgCentralSql.csh real failed (rc=$rc). See $hgclog"
        fi
        log "hgcentral SQL built and pushed."
    fi

    # Step 2: Build userApps (takes ~50 minutes)
    log "Building userApps via doHgDownloadUtils.csh (takes ~50 min)..."
    local utilslog="$LOGDIR/v${BRANCHNN}.doHgDownloadUtils.log"
    run_tcsh "time ./doHgDownloadUtils.csh >& $utilslog"
    local rc=$?
    if [[ $rc -ne 0 ]]; then
        die "doHgDownloadUtils.csh failed (rc=$rc). See $utilslog"
    fi
    log "userApps build complete."

    # Step 3: Tag beta
    log "Tagging beta (dry run first)..."
    local taglog="$LOGDIR/v${BRANCHNN}.tagBeta.log"
    run_tcsh "./tagBeta.csh >& $taglog"

    log "Tagging beta for real..."
    run_tcsh "./tagBeta.csh real >>& $taglog"
    local rc=$?
    if [[ $rc -ne 0 ]]; then
        die "tagBeta.csh real failed (rc=$rc). See $taglog"
    fi
    log "Beta tagged."

    # Step 4: Tag the official release
    log "Tagging official release..."
    if ! $DRY_RUN; then
        cd "$WEEKLYBLD"
        git fetch

        # Find next available subversion tag
        local existing_tags
        existing_tags=$(git tag | grep "v${BRANCHNN}_branch" | sort -t. -k2 -n | tail -1) || true
        local next_sub=1
        if [[ -n "$existing_tags" ]]; then
            local last_sub
            last_sub=$(echo "$existing_tags" | grep -oP '\.\K[0-9]+$') || true
            if [[ -n "$last_sub" ]]; then
                next_sub=$((last_sub + 1))
            fi
        fi
        log "Creating release tag v${BRANCHNN}_branch.${next_sub}"
        run git push origin "origin/v${BRANCHNN}_branch:refs/tags/v${BRANCHNN}_branch.${next_sub}"
        run git fetch
    fi

    # Step 5: Zip source code (takes ~4 min)
    log "Zipping source code..."
    local ziplog="$LOGDIR/v${BRANCHNN}.doZip.log"
    run_tcsh "time ./doZip.csh >& $ziplog"
    local rc=$?
    if [[ $rc -ne 0 ]]; then
        die "doZip.csh failed (rc=$rc). See $ziplog"
    fi
    log "Source zip complete."

    # Step 6: Wait 10 minutes for rsync, then package userApps source
    log "Waiting 10 minutes before userApps.sh (as specified in wiki)..."
    if ! $DRY_RUN; then
        sleep 600
    fi

    log "Running userApps.sh..."
    local uaLog="$LOGDIR/v${BRANCHNN}.userApps.log"
    run_tcsh "time ./userApps.sh >& $uaLog"
    local rc=$?
    if [[ $rc -ne 0 ]]; then
        die "userApps.sh failed (rc=$rc). See $uaLog"
    fi
    log "userApps.sh complete."

    # Step 7: Build release Docker images
    # The amd64 image installs PREBUILT CGIs that browserSetup.sh -b rsyncs from the
    # RR/production cgi-bin module on hgdownload (hgdownload.soe.ucsc.edu::cgi-bin);
    # only arm64 compiles from the beta branch source. Those prebuilt CGIs are put on
    # hgdownload by the external RR/production push (qateam), NOT by these scripts, so
    # we must not build amd64 until that push has propagated or it ships stale,
    # unpatched binaries. Block until hgdownload carries a build at least as new as
    # ours. (The csh one-liner below must run via run_tcsh: `setenv` is csh-only, so
    # running it from bash leaves $stage empty and the image refs become
    # "genomebrowser/server:" while still exiting 0 on the last statement.)
    wait_for_hgdownload_cgis
    log "Building release Docker images..."
    ensure_binfmt
    local dockerdir="$BUILDHOME/v${BRANCHNN}_branch/kent/src/product/installer/docker"
    if [[ -d "$dockerdir" ]]; then
        local dockerlog="$LOGDIR/v${BRANCHNN}.docker-release.log"
        # refs #37350: rm stale local manifest before create, drop --amend to prevent digest accumulation
        run_tcsh "cd $dockerdir && setenv stage v${BRANCHNN} && docker build --no-cache --platform linux/amd64 -t genomebrowser/server:\${stage}-amd64 . && docker push genomebrowser/server:\${stage}-amd64 && docker build --no-cache --platform linux/arm64 -t genomebrowser/server:\${stage}-arm64 . && docker push genomebrowser/server:\${stage}-arm64 && docker manifest rm genomebrowser/server:\${stage} >& /dev/null ; docker manifest create genomebrowser/server:\${stage} genomebrowser/server:\${stage}-amd64 genomebrowser/server:\${stage}-arm64 && docker manifest push genomebrowser/server:\${stage} && docker manifest rm genomebrowser/server:latest >& /dev/null ; docker manifest create genomebrowser/server:latest genomebrowser/server:\${stage}-amd64 genomebrowser/server:\${stage}-arm64 && docker manifest push genomebrowser/server:latest" >& "$dockerlog" || {
            log "WARNING: Docker release build had issues. See $dockerlog (non-fatal)"
        }
        log "Docker release build complete."
    else
        log "WARNING: Docker directory not found - skipping Docker release build"
    fi

    # Refresh kent-rel against the just-pushed release image, then tear down
    # the beta container/image now that v${BRANCHNN} has shipped. refs #37655
    log "Refreshing local kent-rel container..."
    run "$WEEKLYBLD/refresh-instance.sh" rel || \
        log "WARNING: kent-rel refresh failed; container may be stale"
    log "Removing local kent-beta container and image (v${BRANCHNN} has shipped)..."
    run "$WEEKLYBLD/remove-instance.sh" beta || \
        log "WARNING: kent-beta teardown failed; check container/image manually"

    # Step 8: Generate markdown release notes for GitHub
    if ! $DRY_RUN; then
        generate_release_markdown "$BRANCHNN"
    fi

    log "Wrap-up complete for v${BRANCHNN}."
    log "Manual steps remaining:"
    log "  - Push to genome browser store: sudo /cluster/bin/scripts/gbib_gbic_push"
    log "  - Create GitHub release at https://github.com/ucscGenomeBrowser/kent/releases/new"
    log "    Release notes: $WEEKLYBLD/markdownReleaseNotes/v${BRANCHNN}_markdown.txt"
    log "  - Wait 1 day for nightly rsync, then verify hgdownload: https://hgdownload.soe.ucsc.edu/admin/exe/"
    log "  - Send mirror announcement email to genome-mirror@soe.ucsc.edu"
}

##############################################################################
# Main
##############################################################################

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --dry-run)
                DRY_RUN=true
                log "DRY RUN MODE - no changes will be made"
                shift
                ;;
            preview1|preview2|final|wrapup)
                FORCED_PHASE="$1"
                shift
                ;;
            --help|-h)
                echo "Usage: $SCRIPT_NAME [--dry-run] [preview1|preview2|final|wrapup]"
                echo ""
                echo "Runs the CGI build phase for today's date (per Google Calendar),"
                echo "or a forced phase if specified. Stops on any error."
                exit 0
                ;;
            *)
                die "Unknown argument: $1. Use --help for usage."
                ;;
        esac
    done

    log "============================================"
    log "UCSC Genome Browser Automated CGI Build"
    log "============================================"

    # Preflight checks
    acquire_lock

    if [[ "$(hostname -s)" != "hgwdev" ]]; then
        die "Must run on hgwdev (current host: $(hostname -s))"
    fi

    if [[ "$(whoami)" != "build" ]]; then
        die "Must run as 'build' user (current user: $(whoami))"
    fi

    if [[ ! -f "$BUILDENV" ]]; then
        die "buildEnv.csh not found: $BUILDENV"
    fi

    # Read current environment
    read_buildenv

    # Determine phase
    local phase
    if [[ -n "$FORCED_PHASE" ]]; then
        phase="$FORCED_PHASE"
        log "Phase forced to: $phase"
    else
        phase=$(detect_phase)
        log "Auto-detected phase: $phase"
    fi

    # Ensure clean state (except for wrapup which may run on a branch)
    ensure_master_branch
    ensure_clean_git

    # Dispatch
    case "$phase" in
        preview1)
            do_preview1
            ;;
        preview2)
            do_preview2
            ;;
        final)
            do_final
            ;;
        wrapup)
            do_wrapup
            ;;
        *)
            die "Unknown phase: $phase"
            ;;
    esac

    log "============================================"
    log "BUILD PHASE '$phase' COMPLETED SUCCESSFULLY"
    log "============================================"

    # Send success notification. Use PHASE_VER (the version this phase produces),
    # not BRANCHNN, since preview1/preview2 run before BRANCHNN is bumped.
    local ver="${PHASE_VER:-$BRANCHNN}"
    if ! $DRY_RUN; then
        echo "autoBuild.sh completed phase '$phase' for v${ver} successfully at $(date)" \
            | mail -s "AUTOBUILD OK: $phase v${ver}" "${BUILDMEISTEREMAIL:-braney@ucsc.edu}" 2>/dev/null || true
    fi
}

main "$@"

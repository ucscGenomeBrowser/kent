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
#   autoBuild.sh cherrypick [<sha>...]   # build patch: cherry-pick onto v${NN}_branch
#                                        # (ids given here are written fresh to
#                                        #  CherryPickCommits.conf; with none, the
#                                        #  existing conf is used and confirmed if stale)
#   autoBuild.sh --dry-run [phase]   # show what would happen
#
# BUILD PATCHES -- the `cherrypick` phase (autoBuild.sh cherrypick):
#   A build patch cherry-picks approved bugfix commit(s) from master onto
#   v${NN}_branch after the weekly build. Give the commit id(s) on the command
#   line -- `autoBuild.sh cherrypick <sha>...` -- and they are validated and
#   written fresh to CherryPickCommits.conf; or run `autoBuild.sh cherrypick` with
#   no ids to reuse the existing conf, which is then confirmed interactively (y/N)
#   if it looks stale (lists commits already on the branch). Each id is checked to
#   exist and to not be a merge commit (NO merge commits in a cherry-pick).
#   It is a forced-only phase (never auto-detected from the calendar) and is
#   repeatable: each run is one patch round (logs/v${NN}.patch<N>.log), and the
#   checkpoint state file is cleared on success so the next round starts fresh.
#
#   The phase branches on pushedToRR.flag (written by tagBeta at wrap-up, removed
#   by tagNewBranch at the final build -- so "present" == release already shipped):
#     PRE-RR  (flag absent, QA window ~days 16-19): cherry-pick onto the branch,
#       rebuild beta + deploy to hgwbeta so QA can re-test, then git-reports.
#       Wrap-up does all the tagging + artifacts later.
#     POST-RR (flag present, release shipped): additionally re-run the relevant
#       wrap-up steps to regenerate the already-public artifacts -- tagBeta, the
#       next v${NN}_branch.<N> subversion tag (same logic wrap-up uses), doZip,
#       userApps, and the docker release images -- then refresh kent-rel. A
#       reminder to update the Redmine build-patch ticket is emailed BEFORE the
#       long artifact builds.
#   (Reverts are NOT a separate phase: a revert is a revert-commit made on master,
#   reviewed like any change, then cherry-picked onto the branch through this path.)
#
#   The docker release build does NOT depend on the RR push: buildReleaseDocker.sh
#   seeds the amd64 image's CGIs straight from the local hgwdev beta build
#   (/usr/local/apache/cgi-bin-beta) as an overlay -- the same seed overlay-cgi.sh
#   uses for the kent-beta QA container -- so the image carries the just-built beta
#   code immediately, without waiting for that code to propagate to
#   hgdownload.soe.ucsc.edu::cgi-bin via the external RR/production push. arm64
#   compiles the beta branch from source, so it is patched too. (Earlier versions
#   waited on hgdownload; see commit cc9fa7b4ea1 if you ever need that gate back.)
#
#   COMMON.MK CHERRY-PICK GOTCHA: cherryPickCommits.csh does a `git pull` in the
#   64-bit build sandbox ($BUILDDIR/v${NN}_branch/kent) after pushing the cherry-pick.
#   configureSandbox.csh (run at branch creation) PREPENDS USE_BAM=1 / KNETFILE_HOOKS=1
#   to that sandbox's src/inc/common.mk as a permanent uncommitted working-tree change.
#   So if the cherry-picked commit touches inc/common.mk, the pull aborts ("local
#   changes would be overwritten") and the script prints "error updating 64-bit sandbox"
#   -- but the cherry-pick + push to origin/v${NN}_branch ALREADY SUCCEEDED; only the
#   sandbox refresh failed. Fix in the sandbox:
#       git stash push src/inc/common.mk && git pull --ff-only && git stash pop
#   (the flag lines and the incoming change are in different parts of the file, so the
#   pop is clean). This sandbox is not used by doZip or the docker build, so this is
#   hygiene, not a blocker.
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
CHERRYPICK_ARGS=()   # commit ids given on the command line: autoBuild.sh cherrypick <sha>...

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
#
# POST-REBOOT DOCKER FRAGILITY (two separate failure modes; both surface only in
# the docker-testing / docker-release steps after an hgwdev reboot):
#   1. binfmt handlers cleared -> arm64 build dies at the first RUN with
#      "exec /bin/sh: exec format error". ensure_binfmt() below fixes this.
#   2. docker bridge networking broken -> containers on docker0 (172.17.0.0/16)
#      can't reach DNS or the outside, so `apt-get update` inside the build fails
#      with "Temporary failure resolving 'archive.ubuntu.com'" and then
#      "E: Unable to locate package wget/rsync". The reboot leaves docker's
#      iptables MASQUERADE/FORWARD rules uninstalled. Diagnose (no sudo):
#          docker run --rm alpine sh -c "nc -zv 10.1.1.10 53; nc -zv 8.8.8.8 443"
#      If both time out but the host pings 10.1.1.10 fine, it's the bridge.
#      Fix (needs sudo): `sudo systemctl restart docker` reinstalls the rules.
#      Then re-run the failed docker step.
ensure_binfmt() {
    log "Registering QEMU binfmt handlers for cross-arch docker builds..."
    if run docker run --privileged --rm tonistiigi/binfmt --install all >/dev/null 2>&1; then
        log "OK: binfmt handlers registered"
    else
        log "WARNING: binfmt registration failed; arm64 docker builds may fail"
    fi
}

# Verify the GitHub CLI is authenticated. doZip.csh uses `gh release create/upload`
# to attach the submodule-complete source archives to the GitHub release (#37741);
# if the build user's gh token has expired, that step fails LATE (after the ~1hr of
# hgcentral/userApps/tag/zip work) and only WARNS, so the release silently ships
# without its assets. Checking up front turns that into a clear, immediate stop.
# Called at the start of do_wrapup and the post-RR cherry-pick regen path (both run
# doZip). Not a checkpointed step -- it is a cheap precondition that must re-verify
# on every (re-)run.
ensure_gh_auth() {
    if $DRY_RUN; then
        log "(dry-run) would verify gh auth status"
        return 0
    fi
    if gh auth status >/dev/null 2>&1; then
        log "OK: gh authenticated"
    else
        die "gh is not authenticated (gh auth status failed). Run 'gh auth login' as
    the build user, then re-run. doZip.csh needs gh to attach the source archives to
    the GitHub release ($WEEKLYBLD/doZip.csh, refs #37741)."
    fi
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

# If this dies with a missing htslib header (e.g.
# "../../inc/bamFile.h: htslib/sam.h: No such file or directory"), it is a
# transient parallel-make (-j) race -- htslib hadn't finished building when a
# dependent target needed its header. It is NOT a real environment/header
# problem: do not investigate htslib setup or compare sandboxes. Just re-run
# autoBuild.sh; the checkpoint resumes here and the make succeeds.
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

# --- Wrap-up steps (decomposed for resumability) ---
#
# do_wrapup was historically a straight-line sequence with NO checkpointing. If
# it died partway through, re-running `autoBuild.sh wrapup` re-did every earlier
# step: it re-pushed hgcentral, rebuilt userApps, re-tagged beta, and -- worst --
# created a DUPLICATE release tag (v${NN}_branch.2). The fix was to finish the
# remaining steps by hand. These steps now run under the same checkpoint
# framework as preview1/preview2/final: the non-idempotent steps (hgcentral push,
# tagBeta, tag-release) run exactly once, and a re-run resumes at the first
# incomplete step instead of redoing committed work.

wrapup_hgcentral_sql() {
    log "Building hgcentral SQL (dry run first)..."
    local hgclog="$LOGDIR/v${BRANCHNN}.buildHgCentralSql.log"
    run_tcsh "./buildHgCentralSql.csh >& $hgclog"
    log "hgcentral dry-run complete. Checking for differences..."
    if grep -q "No differences" "$hgclog" 2>/dev/null; then
        log "No hgcentral differences - skipping real hgcentral push."
    else
        log "hgcentral differences found. Running for real..."
        run_tcsh "./buildHgCentralSql.csh real >>& $hgclog" || \
            die "buildHgCentralSql.csh real failed. See $hgclog"
        log "hgcentral SQL built and pushed."
    fi
    return 0
}

# Build userApps (takes ~50 minutes)
wrapup_userapps_utils() {
    log "Building userApps via doHgDownloadUtils.csh (takes ~50 min)..."
    local utilslog="$LOGDIR/v${BRANCHNN}.doHgDownloadUtils.log"
    run_tcsh "time ./doHgDownloadUtils.csh >& $utilslog" || \
        die "doHgDownloadUtils.csh failed. See $utilslog"
    log "userApps build complete."
    return 0
}

wrapup_tag_beta() {
    log "Tagging beta (dry run first)..."
    local taglog="$LOGDIR/v${BRANCHNN}.tagBeta.log"
    run_tcsh "./tagBeta.csh >& $taglog"
    log "Tagging beta for real..."
    run_tcsh "./tagBeta.csh real >>& $taglog" || \
        die "tagBeta.csh real failed. See $taglog"
    log "Beta tagged."
    return 0
}

# Push the next v${BRANCHNN}_branch.<N> release subversion tag, pointing at the
# current tip of origin/v${BRANCHNN}_branch. Shared by wrap-up (initial release
# tag) and the cherry-pick phase (post-RR patch re-tag). NOT idempotent on its
# own (a re-run would create a duplicate .<N> tag), so every caller runs it under
# the checkpoint framework so it fires exactly once per phase.
tag_next_subversion() {
    if $DRY_RUN; then
        log "(dry-run) would create the next v${BRANCHNN}_branch.N release tag"
        return 0
    fi
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
    run git push origin "origin/v${BRANCHNN}_branch:refs/tags/v${BRANCHNN}_branch.${next_sub}" || \
        die "Failed to push release tag v${BRANCHNN}_branch.${next_sub}"
    run git fetch
    return 0
}

# Tag the official release. NOT idempotent on its own (a re-run would create a
# duplicate v${NN}_branch.<next> tag), so the checkpoint must ensure it runs
# exactly once -- which it now does.
wrapup_tag_release() {
    log "Tagging official release..."
    tag_next_subversion
}

# Zip source code (takes ~4 min)
wrapup_zip() {
    log "Zipping source code..."
    local ziplog="$LOGDIR/v${BRANCHNN}.doZip.log"
    run_tcsh "time ./doZip.csh >& $ziplog" || \
        die "doZip.csh failed. See $ziplog"
    log "Source zip complete."
    return 0
}

# Wait 10 minutes for rsync, then package userApps source
wrapup_userapps_src() {
    log "Waiting 10 minutes before userApps.sh (as specified in wiki)..."
    if ! $DRY_RUN; then
        sleep 600
    fi
    log "Running userApps.sh..."
    local uaLog="$LOGDIR/v${BRANCHNN}.userApps.log"
    run_tcsh "time ./userApps.sh >& $uaLog" || \
        die "userApps.sh failed. See $uaLog"
    log "userApps.sh complete."
    return 0
}

# Build release Docker images via buildReleaseDocker.sh.
# amd64 = base image (browserSetup public CGIs) + a local beta-CGI overlay from
# /usr/local/apache/cgi-bin-beta, so the image carries the just-built beta code
# WITHOUT waiting for the RR/production push to propagate to hgdownload -- the same
# seed overlay-cgi.sh uses for the kent-beta QA container. arm64 compiles the beta
# branch from source. See the BUILD PATCHES note in the header. buildReleaseDocker.sh
# verifies the overlay landed (in-image hgTracks == cgi-bin-beta) before pushing.
# Fatal on failure: wrap-up is checkpointed, so a retry resumes at this step, and
# buildReleaseDocker.sh is idempotent (--no-cache rebuild + re-push + manifest
# recreate). On apt-get DNS errors or an arm64 "exec format error", see the
# post-reboot docker fragility note at ensure_binfmt.
wrapup_docker_release() {
    log "Building release Docker images (amd64 beta-overlay + arm64 beta-source)..."
    ensure_binfmt
    local dockerlog="$LOGDIR/v${BRANCHNN}.docker-release.log"
    run "$WEEKLYBLD/buildReleaseDocker.sh" "v${BRANCHNN}" >& "$dockerlog" || \
        die "Docker release build failed. See $dockerlog"
    log "Docker release build complete."
    return 0
}

# Refresh kent-rel against the just-pushed release image, then tear down the beta
# container/image now that v${BRANCHNN} has shipped. refs #37655. Container
# hygiene only, so a failure here warns but does not abort the phase.
wrapup_refresh_containers() {
    log "Refreshing local kent-rel container..."
    run "$WEEKLYBLD/refresh-instance.sh" rel || \
        log "WARNING: kent-rel refresh failed; container may be stale"
    log "Removing local kent-beta container and image (v${BRANCHNN} has shipped)..."
    run "$WEEKLYBLD/remove-instance.sh" beta || \
        log "WARNING: kent-beta teardown failed; check container/image manually"
    return 0
}

# Generate markdown release notes for GitHub
wrapup_release_markdown() {
    if ! $DRY_RUN; then
        generate_release_markdown "$BRANCHNN"
    fi
    return 0
}

do_wrapup() {
    log "========== PHASE: WRAP-UP =========="
    read_buildenv
    PHASE_VER=$BRANCHNN

    log "Running wrap-up for v${BRANCHNN}"

    # Precondition: gh must be authenticated now, so a stale token fails here
    # rather than after the ~1hr of work leading up to doZip's GitHub upload.
    ensure_gh_auth

    state_init wrapup "$PHASE_VER"

    step hgcentral-sql    wrapup_hgcentral_sql
    step userapps-utils   wrapup_userapps_utils
    step tag-beta         wrapup_tag_beta
    step tag-release      wrapup_tag_release
    # release-markdown must run before zip: doZip.csh attaches the curated
    # markdown notes to the GitHub release it creates, so they must exist first.
    step release-markdown wrapup_release_markdown
    step zip              wrapup_zip
    step userapps-src     wrapup_userapps_src
    step docker-release   wrapup_docker_release
    step refresh-containers wrapup_refresh_containers

    log "Wrap-up complete for v${BRANCHNN}."
    log "Manual steps remaining:"
    log "  - Push to genome browser store: sudo /cluster/bin/scripts/gbib_gbic_push"
    log "  - Verify the GitHub release doZip.csh created (with submodule-complete"
    log "    source archives attached): https://github.com/ucscGenomeBrowser/kent/releases"
    log "    Edit its notes if needed; source: $WEEKLYBLD/markdownReleaseNotes/v${BRANCHNN}_markdown.txt"
    log "    Note: GitHub's own auto-generated 'Source code' zip/tar.gz omit submodules;"
    log "    users should download the attached kent.src.zip / kent.src.tar.gz instead."
    log "  - Wait 1 day for nightly rsync, then verify hgdownload: https://hgdownload.soe.ucsc.edu/admin/exe/"
    log "  - Send mirror announcement email to genome-mirror@soe.ucsc.edu"
}

##############################################################################
# Phase: Cherry-pick (build patch)
#
# Forced-only (never calendar-detected) and repeatable: each invocation is one
# "patch round". See the BUILD PATCHES note in the header for the pre-RR vs
# post-RR behavior. The commits to apply live in CherryPickCommits.conf, one SHA
# per line, no merge commits.
##############################################################################

# Log file for the current patch round (set in do_cherrypick).
CHERRYPICK_LOG=""

# Dry-run pass: cherryPickCommits.csh with no "real" arg verifies each commit in
# CherryPickCommits.conf exists and shows its diffstat; it makes no changes and
# errors out if the conf file is missing or empty.
cherrypick_apply_dryrun() {
    run_tcsh "./cherryPickCommits.csh >& $CHERRYPICK_LOG"
}

# Real pass: cherry-pick each commit onto v${BRANCHNN}_branch, push the branch to
# origin, log to cherryPicks.log, email, and pull the 64-bit build sandbox.
cherrypick_apply_commits() {
    run_tcsh "./cherryPickCommits.csh real >>& $CHERRYPICK_LOG" || \
        die "cherryPickCommits.csh real failed. See $CHERRYPICK_LOG.
    - If it stopped on a CONFLICT: resolve + commit + 'git push origin v${BRANCHNN}_branch'
      by hand, then (since the pick already landed) mark step 'apply-commits' done by
      appending it to $STATE_FILE and re-run to resume at the next step.
    - If it said 'error updating 64-bit sandbox' and the commit touched inc/common.mk:
      the cherry-pick + push ALREADY SUCCEEDED (only the sandbox refresh failed). Fix in
      the sandbox with: git stash push src/inc/common.mk && git pull --ff-only && git stash
      pop  -- then mark 'apply-commits' done in $STATE_FILE and re-run.
    - Otherwise fix CherryPickCommits.conf and re-run."
}

# Email the build-meister a reminder to update the Redmine build-patch ticket.
# The script runs unattended, so it cannot post to Redmine itself (posting needs
# human approval per the redmine skill) -- it just reminds. Sent BEFORE the long
# artifact builds, per the documented build-patch ordering.
cherrypick_redmine_reminder() {
    if $DRY_RUN; then
        log "(dry-run) would email a reminder to update the Redmine build-patch ticket"
        return 0
    fi
    echo "Cherry-pick(s) applied to v${BRANCHNN}_branch (see $CHERRYPICK_LOG). REMINDER: update the Redmine build-patch ticket with the cherry-picked commit(s), per the redmine skill (echo the comment for approval before posting)." \
        | mail -s "REMINDER: update Redmine build-patch ticket (v${BRANCHNN})" "${BUILDMEISTEREMAIL:-braney@ucsc.edu}" 2>/dev/null || true
    return 0
}

# Post-RR only: refresh the kent-rel container against the just-rebuilt release
# image. (kent-beta was already torn down at wrap-up, so it is not touched here.)
cherrypick_refresh_rel() {
    run "$WEEKLYBLD/refresh-instance.sh" rel || \
        log "WARNING: kent-rel refresh failed; container may be stale"
    return 0
}

# Preflight (fresh round only): settle exactly which commit ids will be applied
# and sanity-check them, so cherryPickCommits.csh isn't handed a stale or bad list.
#
#   - Command-line ids (autoBuild.sh cherrypick <sha>...) are authoritative: they
#     are validated and written FRESH to CherryPickCommits.conf (overwriting any
#     leftover contents). An id already on the branch is a hard error here (it was
#     typed explicitly, so it's a mistake, not ambiguity).
#   - No command-line ids => use the existing CherryPickCommits.conf, but treat it
#     as suspect: if any listed commit is already on v${BRANCHNN}_branch the file
#     "looks stale", and we ask for interactive y/N confirmation before proceeding
#     (refusing rather than hanging if there is no terminal).
#
# Every id is also checked to exist and to not be a merge commit either way.
cherrypick_prepare_commits() {
    local conf="$WEEKLYBLD/CherryPickCommits.conf"
    local -a shas=()
    local from_cmdline=false

    if (( ${#CHERRYPICK_ARGS[@]} > 0 )); then
        from_cmdline=true
        shas=("${CHERRYPICK_ARGS[@]}")
        log "Commit id(s) from the command line: ${shas[*]}"
    else
        [[ -s "$conf" ]] || die "No commit ids given and $conf is missing/empty. Run: $SCRIPT_NAME cherrypick <sha>..."
        local tok
        for tok in $(grep -vE '^[[:space:]]*(#|$)' "$conf" || true); do shas+=("$tok"); done
        (( ${#shas[@]} > 0 )) || die "$conf has no commit ids. Run: $SCRIPT_NAME cherrypick <sha>..."
        log "Commit id(s) from $conf: ${shas[*]}"
    fi

    cd "$WEEKLYBLD"
    run git fetch -q origin || log "WARNING: git fetch failed; staleness check uses possibly-stale refs"
    local branchref="origin/v${BRANCHNN}_branch"
    local have_branch=false
    if git rev-parse -q --verify "$branchref" >/dev/null 2>&1; then
        have_branch=true
    else
        log "WARNING: $branchref not found; cannot check whether commits are already on the branch."
    fi

    local stale=false sha short subj
    for sha in "${shas[@]}"; do
        git cat-file -e "${sha}^{commit}" 2>/dev/null || \
            die "Commit '$sha' not found in the repo. Fetch/pull master, or fix the id."
        if git rev-parse -q --verify "${sha}^2" >/dev/null 2>&1; then
            die "Commit '$sha' is a MERGE commit -- do not cherry-pick merges (see cherryPickCommits.csh). Use the underlying non-merge commit(s)."
        fi
        short=$(git rev-parse --short "$sha")
        subj=$(git log -1 --format=%s "$sha")
        if $have_branch && git merge-base --is-ancestor "$sha" "$branchref" 2>/dev/null; then
            log "  STALE  $short  (already on v${BRANCHNN}_branch)  $subj"
            stale=true
        else
            log "  new    $short  $subj"
        fi
    done

    if $stale; then
        if $from_cmdline; then
            die "Commit id(s) above are already on v${BRANCHNN}_branch. Nothing to do, or fix the list."
        fi
        log "CherryPickCommits.conf looks STALE (lists commit(s) already on v${BRANCHNN}_branch)."
        if [[ ! -t 0 ]]; then
            die "Refusing to proceed with a stale conf non-interactively. Re-run in a terminal, or pass fresh ids: $SCRIPT_NAME cherrypick <sha>..."
        fi
        if $DRY_RUN; then
            log "(dry-run) would prompt y/N to proceed with the stale conf"
        else
            local ans=""
            read -r -p "Proceed with this list anyway? [y/N] " ans || true
            case "$ans" in
                [yY]|[yY][eE][sS]) log "Proceeding at user confirmation." ;;
                *) die "Aborted (stale CherryPickCommits.conf). Update it, or pass ids: $SCRIPT_NAME cherrypick <sha>..." ;;
            esac
        fi
    fi

    # Command-line ids become the conf that cherryPickCommits.csh reads.
    if $from_cmdline; then
        if $DRY_RUN; then
            log "(dry-run) would write ${#shas[@]} commit id(s) to $conf"
        else
            printf '%s\n' "${shas[@]}" > "$conf"
            log "Wrote ${#shas[@]} commit id(s) to $conf"
        fi
    fi
    return 0
}

do_cherrypick() {
    log "========== PHASE: CHERRY-PICK (build patch) =========="
    read_buildenv
    PHASE_VER=$BRANCHNN
    state_init cherrypick "$PHASE_VER"

    # Repeatable phase: pick the next patch-round number for log names and persist
    # it in the state file so a resume reuses the same round (and log file).
    local patchn
    patchn=$(grep -oP '^patchn=\K[0-9]+' "$STATE_FILE" 2>/dev/null | head -1 || true)
    if [[ -z "$patchn" ]]; then
        local n=1
        while [[ -e "$LOGDIR/v${BRANCHNN}.patch${n}.log" ]]; do n=$((n + 1)); done
        patchn=$n
        $DRY_RUN || echo "patchn=${patchn}" >> "$STATE_FILE"
    fi
    CHERRYPICK_LOG="$LOGDIR/v${BRANCHNN}.patch${patchn}.log"
    log "Build-patch round ${patchn} for v${BRANCHNN}; log: $CHERRYPICK_LOG"

    # pushedToRR.flag present => release already shipped => regenerate the public
    # artifacts (re-run the relevant wrap-up steps). Absent => still in the QA
    # window; just land the patch + rebuild beta, wrap-up tags/artifacts later.
    local post_rr=false
    if [[ -e "$WEEKLYBLD/pushedToRR.flag" ]]; then
        post_rr=true
        log "pushedToRR.flag present -> POST-RR patch: will regenerate release artifacts."
        # This path re-runs doZip (GitHub upload), so require gh auth up front too.
        ensure_gh_auth
    else
        log "pushedToRR.flag absent -> PRE-RR patch (QA window): landing patch + rebuilding beta only."
    fi

    # Settle + validate the commit list before applying -- but only on a FRESH
    # round. On a resume, apply-commits may already have landed commits, which
    # would make the "already on the branch" staleness check fire on our own work.
    if step_is_done apply-commits; then
        log "Resuming after apply-commits; skipping commit-list validation."
    else
        cherrypick_prepare_commits
    fi

    # --- Core patch: applies in both cases ---
    step apply-dryrun     cherrypick_apply_dryrun
    step apply-commits    cherrypick_apply_commits
    step redmine-reminder cherrypick_redmine_reminder    # before the long builds
    step build-beta       final_build_beta               # reused from the final phase
    step deploy-beta      final_deploy_beta               # rsync cgi-bin-beta -> hgwbeta
    step git-reports      final_git_reports

    if $post_rr; then
        # Re-run the relevant wrap-up steps to regenerate already-public artifacts.
        step tag-beta         wrapup_tag_beta
        step tag-subversion   tag_next_subversion
        step release-markdown wrapup_release_markdown
        step zip              wrapup_zip
        step userapps-utils   wrapup_userapps_utils
        step userapps-src     wrapup_userapps_src
        step docker-release   wrapup_docker_release
        step refresh-rel      cherrypick_refresh_rel
    else
        step refresh-beta     final_refresh_beta          # refresh kent-beta for QA
    fi

    # Repeatable phase: clear the state file on FULL success so the next patch
    # round starts fresh. (A failed round leaves it in place so a re-run resumes
    # at the first incomplete step -- same as every other phase.)
    $DRY_RUN || rm -f "$STATE_FILE"

    log "Cherry-pick build patch (round ${patchn}) complete for v${BRANCHNN}."
    if $post_rr; then
        log "Public artifacts regenerated. Remaining manual steps: verify the GitHub"
        log "release, push to the genome browser store, and confirm hgdownload."
    else
        log "Patch is on v${BRANCHNN}_branch and beta; wrap-up will tag + build artifacts later."
    fi
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
            preview1|preview2|final|wrapup|cherrypick)
                FORCED_PHASE="$1"
                shift
                ;;
            --help|-h)
                echo "Usage: $SCRIPT_NAME [--dry-run] [preview1|preview2|final|wrapup]"
                echo "       $SCRIPT_NAME [--dry-run] cherrypick [<sha>...]"
                echo ""
                echo "Runs the CGI build phase for today's date (per Google Calendar),"
                echo "or a forced phase if specified. Stops on any error."
                echo ""
                echo "cherrypick is forced-only (never auto-detected): it applies build-patch"
                echo "commit(s) onto v\${NN}_branch. Give the commit id(s) on the command line"
                echo "(they are written fresh to CherryPickCommits.conf), or run it with no ids"
                echo "to use the existing CherryPickCommits.conf -- in which case, if that file"
                echo "looks stale (lists commits already on the branch), it asks for y/N first."
                exit 0
                ;;
            *)
                # After 'cherrypick', any remaining non-flag args are commit ids.
                if [[ "$FORCED_PHASE" == "cherrypick" ]]; then
                    CHERRYPICK_ARGS+=("$1")
                    shift
                else
                    die "Unknown argument: $1. Use --help for usage."
                fi
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
        cherrypick)
            do_cherrypick
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

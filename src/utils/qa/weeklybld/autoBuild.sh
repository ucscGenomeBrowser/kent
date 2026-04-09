#!/bin/bash
#
# autoBuild.sh - Fully automated CGI build process
#
# Runs the appropriate build phase (preview1, preview2, final build,
# or wrap-up) based on the schedule in buildSchedule.txt.
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
set -eEu -o pipefail

##############################################################################
# Configuration
##############################################################################
SCRIPT_NAME="$(basename "$0")"
SCHEDULE_FILE="$WEEKLYBLD/buildSchedule.txt"
WEEKLYBLD="/hive/groups/browser/newBuild/kent/src/utils/qa/weeklybld"
BUILDENV="$WEEKLYBLD/buildEnv.csh"
LOGDIR="$WEEKLYBLD/logs"
LOCKFILE="/tmp/autoBuild.lock"
DRY_RUN=false
FORCED_PHASE=""

##############################################################################
# Helpers
##############################################################################

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$SCRIPT_NAME] $*"
}

die() {
    log "FATAL: $*"
    log "Build ABORTED."
    if ! $DRY_RUN; then
        log "Sending alert email."
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
# Determine which phase to run from the schedule
##############################################################################

detect_phase() {
    local ds
    ds=$(date "+%F")
    local entry
    entry=$(grep "^${ds}" "$SCHEDULE_FILE" 2>/dev/null | head -1 | cut -f2) || true

    if [[ -z "$entry" ]]; then
        die "No entry for today ($ds) in $SCHEDULE_FILE. Is the schedule up to date?"
    fi

    log "Schedule entry for $ds: $entry" >&2

    if echo "$entry" | grep -qi "preview 1"; then
        echo "preview1"
    elif echo "$entry" | grep -qi "preview 2"; then
        echo "preview2"
    elif echo "$entry" | grep -qi "final build"; then
        echo "final"
    else
        die "Unrecognized schedule entry: '$entry'"
    fi
}

##############################################################################
# Phase: Preview 1 (Day 1)
##############################################################################

do_preview1() {
    log "========== PHASE: PREVIEW 1 =========="
    read_buildenv

    local NEXTNN=$((BRANCHNN + 1))
    # The new REVIEWDAY is today, the old REVIEWDAY becomes LASTREVIEWDAY
    local new_REVIEWDAY
    new_REVIEWDAY=$(date "+%F")
    local new_LASTREVIEWDAY="$REVIEWDAY"

    log "Updating buildEnv.csh: LASTREVIEWDAY=$new_LASTREVIEWDAY -> REVIEWDAY=$new_REVIEWDAY (v${NEXTNN} preview)"

    # Validate: new dates must be sane
    if [[ "$new_REVIEWDAY" < "$new_LASTREVIEWDAY" ]]; then
        die "Date sanity check failed: new REVIEWDAY ($new_REVIEWDAY) < new LASTREVIEWDAY ($new_LASTREVIEWDAY)"
    fi

    if ! $DRY_RUN; then
        # Edit buildEnv.csh - update LASTREVIEWDAY and REVIEWDAY
        sed -i \
            -e "s|^setenv LASTREVIEWDAY .*|setenv LASTREVIEWDAY ${new_LASTREVIEWDAY}                     # v${BRANCHNN} preview|" \
            -e "s|^setenv REVIEWDAY .*|setenv REVIEWDAY ${new_REVIEWDAY}                     # v${NEXTNN} preview|" \
            "$BUILDENV"
    fi
    log "buildEnv.csh updated"

    # Verify the edit
    if ! grep -q "setenv REVIEWDAY ${new_REVIEWDAY}" "$BUILDENV"; then
        $DRY_RUN || die "buildEnv.csh edit verification failed for REVIEWDAY"
    fi
    if ! grep -q "setenv LASTREVIEWDAY ${new_LASTREVIEWDAY}" "$BUILDENV"; then
        $DRY_RUN || die "buildEnv.csh edit verification failed for LASTREVIEWDAY"
    fi

    # Re-source and verify
    run_tcsh "source $BUILDENV && env | egrep VIEWDAY"

    # Commit
    run_tcsh "cd $WEEKLYBLD && git add buildEnv.csh && git commit -m 'v${NEXTNN} preview1 (automated)' buildEnv.csh && git push"

    # Run doNewReview.csh (dry-run first, then real)
    log "Running doNewReview.csh dry-run check..."
    run_tcsh "./doNewReview.csh"

    log "Running doNewReview.csh for real..."
    local logfile="$LOGDIR/v${NEXTNN}.doNewRev.log"
    run_tcsh "./doNewReview.csh real >& $logfile"
    local rc=$?

    if [[ $rc -ne 0 ]]; then
        die "doNewReview.csh failed with exit code $rc. See $logfile"
    fi

    # Sanity check the log for errors
    if grep -qi "failed\|error" "$logfile" 2>/dev/null | grep -vi "0 errors" | head -5 | grep -q .; then
        log "WARNING: Possible errors in $logfile - review manually"
    fi

    log "Preview 1 complete."
}

##############################################################################
# Phase: Preview 2 (Day 8)
##############################################################################

do_preview2() {
    log "========== PHASE: PREVIEW 2 =========="
    read_buildenv

    local NEXTNN=$((BRANCHNN + 1))
    local new_REVIEW2DAY
    new_REVIEW2DAY=$(date "+%F")
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
    fi
    log "buildEnv.csh updated"

    if ! grep -q "setenv REVIEW2DAY  ${new_REVIEW2DAY}" "$BUILDENV"; then
        $DRY_RUN || die "buildEnv.csh edit verification failed for REVIEW2DAY"
    fi

    run_tcsh "source $BUILDENV && env | grep 2DAY"

    run_tcsh "cd $WEEKLYBLD && git add buildEnv.csh && git commit -m 'v${NEXTNN} preview2 (automated)' buildEnv.csh && git push"

    # Run doNewReview2.csh
    log "Running doNewReview2.csh dry-run check..."
    run_tcsh "./doNewReview2.csh"

    log "Running doNewReview2.csh for real..."
    local logfile="$LOGDIR/v${NEXTNN}.doNewRev2.log"
    run_tcsh "./doNewReview2.csh real >& $logfile"
    local rc=$?

    if [[ $rc -ne 0 ]]; then
        die "doNewReview2.csh failed with exit code $rc. See $logfile"
    fi

    # Run the preview2 tables test robot
    log "Running preview2TablesTestRobot.csh (takes ~1h40m)..."
    local tableslog="$LOGDIR/v${NEXTNN}.preview2.hgTables.log"
    run_tcsh "time ./preview2TablesTestRobot.csh >& $tableslog"
    local rc2=$?

    if [[ $rc2 -ne 0 ]]; then
        die "preview2TablesTestRobot.csh failed with exit code $rc2. See $tableslog"
    fi

    log "Preview 2 complete."
}

##############################################################################
# Phase: Final Build (Day 15)
##############################################################################

do_final() {
    log "========== PHASE: FINAL BUILD =========="
    read_buildenv

    # Check SSH logins first
    log "Checking SSH logins to remote hosts..."
    run bash "$WEEKLYBLD/checkLogins.sh" 2>&1 | tee /tmp/autoBuild_checkLogins.log
    if grep -qi "failed" /tmp/autoBuild_checkLogins.log; then
        die "SSH login check failed. Fix SSH keys before proceeding. See /tmp/autoBuild_checkLogins.log"
    fi
    log "OK: All SSH logins successful"

    # Compute new values
    local new_BRANCHNN=$((BRANCHNN + 1))
    local new_TODAY
    new_TODAY=$(date "+%F")
    local new_LASTWEEK="$TODAY"

    log "Updating buildEnv.csh: BRANCHNN=$BRANCHNN -> $new_BRANCHNN, LASTWEEK=$new_LASTWEEK, TODAY=$new_TODAY"

    # Sanity checks
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
    fi
    log "buildEnv.csh updated"

    # Verify edits
    if ! $DRY_RUN; then
        grep -q "setenv BRANCHNN ${new_BRANCHNN}" "$BUILDENV" || die "BRANCHNN edit verification failed"
        grep -q "setenv TODAY ${new_TODAY}" "$BUILDENV" || die "TODAY edit verification failed"
        grep -q "setenv LASTWEEK ${new_LASTWEEK}" "$BUILDENV" || die "LASTWEEK edit verification failed"
    fi

    run_tcsh "source $BUILDENV && env | egrep 'DAY|NN|WEEK'"

    # Commit
    run_tcsh "cd $WEEKLYBLD && git add buildEnv.csh && git commit -m 'v${new_BRANCHNN} final build (automated)' buildEnv.csh && git push"

    # Now BRANCHNN has changed - re-read
    read_buildenv
    log "After re-read: BRANCHNN=$BRANCHNN"

    # Run doNewBranch.csh dry-run
    log "Running doNewBranch.csh dry-run check..."
    run_tcsh "./doNewBranch.csh"

    # Run for real (~1 hour)
    log "Running doNewBranch.csh for real (takes ~1 hour)..."
    local logfile="$LOGDIR/v${BRANCHNN}.doNewBranch.log"
    run_tcsh "./doNewBranch.csh real >& $logfile"
    local rc=$?

    if [[ $rc -ne 0 ]]; then
        die "doNewBranch.csh failed with exit code $rc. See $logfile"
    fi

    # Verify success artifacts
    if ! $DRY_RUN; then
        if [[ ! -f "$WEEKLYBLD/GitReports.ok" ]]; then
            log "WARNING: GitReports.ok not found - git reports may have had issues"
        else
            log "OK: GitReports.ok exists"
        fi

        # Check CGI timestamps in beta
        log "Checking CGI timestamps in cgi-bin-beta..."
        local newest_cgi
        newest_cgi=$(ls -lt /usr/local/apache/cgi-bin-beta/ 2>/dev/null | head -5) || true
        log "Newest CGIs in beta:\n$newest_cgi"

        # Verify the CGIs were built today
        local today_date
        today_date=$(date "+%b %e" | sed 's/  / /')  # e.g. "Mar 17"
        if ! echo "$newest_cgi" | grep -q "$(date '+%b')" 2>/dev/null; then
            log "WARNING: CGIs in cgi-bin-beta may not have been updated today. Check manually."
        fi
    fi

    # Run robots in background (takes 6+ hours, wiki says don't wait)
    log "Starting doRobots.csh (runs for 6+ hours in background)..."
    local robotlog="$LOGDIR/v${BRANCHNN}.robots.log"
    if ! $DRY_RUN; then
        nohup /bin/tcsh -c "source $BUILDENV && cd $WEEKLYBLD && ./doRobots.csh >& $robotlog" &
        local robot_pid=$!
        log "doRobots.csh started in background (PID $robot_pid). Log: $robotlog"
    fi

    # Docker testing image build
    log "Building testing Docker images..."
    local dockerdir
    dockerdir="$BUILDHOME/v${BRANCHNN}_branch/kent/src/product/installer/docker"
    if [[ -d "$dockerdir" ]]; then
        local dockerlog="$LOGDIR/v${BRANCHNN}.docker-testing.log"
        # refs #37350: rm stale local manifest before create, drop --amend to prevent digest accumulation
        run_tcsh "cd $dockerdir && setenv stage testing && docker build --no-cache --platform linux/amd64 -t genomebrowser/server:\${stage}-amd64 . && docker push genomebrowser/server:\${stage}-amd64 && docker build --no-cache --platform linux/arm64 -t genomebrowser/server:\${stage}-arm64 . && docker push genomebrowser/server:\${stage}-arm64 && docker manifest rm genomebrowser/server:\${stage} >& /dev/null ; docker manifest create genomebrowser/server:\${stage} genomebrowser/server:\${stage}-amd64 genomebrowser/server:\${stage}-arm64 && docker manifest push genomebrowser/server:\${stage}" >& "$dockerlog" || {
            log "WARNING: Docker testing build had issues. See $dockerlog (non-fatal, continuing)"
        }
        log "Docker testing build complete (or skipped on error)."
    else
        log "WARNING: Docker directory not found at $dockerdir - skipping Docker build"
    fi

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
        | sed 's| \. [A-Z][a-z]*$||' \
        | sed 's|[[:space:]]*$||' \
        | sed 's|^|<li>|; s|$|</li>|' \
        | sed '1s|^|<ul>\n|; $s|$|\n</ul>|' \
        > "$outfile"

    log "Markdown release notes written to $outfile"
}

##############################################################################
# Phase: Wrap-up (Day 23, after push to RR)
##############################################################################

do_wrapup() {
    log "========== PHASE: WRAP-UP =========="
    read_buildenv

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
    log "Building release Docker images..."
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
                echo "Runs the CGI build phase for today's date (per buildSchedule.txt),"
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

    if [[ ! -f "$SCHEDULE_FILE" ]]; then
        die "Schedule file not found: $SCHEDULE_FILE"
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

    # Send success notification
    if ! $DRY_RUN; then
        echo "autoBuild.sh completed phase '$phase' for v${BRANCHNN} successfully at $(date)" \
            | mail -s "AUTOBUILD OK: $phase v${BRANCHNN}" "${BUILDMEISTEREMAIL:-braney@ucsc.edu}" 2>/dev/null || true
    fi
}

main "$@"

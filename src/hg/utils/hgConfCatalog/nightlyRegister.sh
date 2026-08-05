#!/bin/bash
# Keep hgConfCatalog.py's mechanical fields true without asking a developer for
# anything, and report only what needs a person's judgement.  Refs #37925.
#
# The premise is that a developer adding an hg.conf read should not have to know
# this catalog exists.  Asking them to did not work: hubSpaceLockTimeout arrived
# as one supporting line inside an unrelated bugfix and nothing noticed until
# somebody ran --reconcile by hand a day later.  So this job writes down the
# facts itself, nightly, then commits and pushes them.  It leaves exactly one
# thing for a human: deciding what each new setting is for, and whether a
# boolean is a release gate or a mirror knob.  Those two are refused on
# purpose; see auto_register()'s comments in hgConfCatalog.py.
#
# It runs in the weekly build's own tree, and that tree is shared with a build
# process that will not tolerate surprises, so three rules are absolute:
#
#   1. master only.  cherryPickCommits.csh and tagBeta.csh both cd into
#      $BUILDHOME/kent/src/utils/qa/weeklybld and check out v${NN}_branch or
#      beta before coming back to master.  Committing during that window would
#      put an unreviewed row on a release branch and push it.  Release branches
#      belong to the build process alone, so if HEAD is not master this job does
#      nothing at all and tries again tomorrow.
#   2. never leave the tree dirty.  autoBuild.sh's ensure_clean_git() aborts the
#      whole build on any uncommitted change in that tree.  Every exit path here
#      either commits or restores the file, via the trap below.
#   3. never run beside a build.  /tmp/autoBuild.lock means one is in progress.
#
# Everything it writes is regenerable from the tree, so on any doubt it throws
# its own work away rather than leave a mess for the build to trip over.
#
# Quiet when there is nothing to say, because cron mails whatever this prints.

set -euo pipefail

BUILDHOME=${BUILDHOME:-/hive/groups/browser/newBuild}
CHECKOUT=${HGCONF_NIGHTLY_CHECKOUT:-$BUILDHOME/kent}
LOCKFILE=${AUTOBUILD_LOCKFILE:-/tmp/autoBuild.lock}
RELPATH=src/hg/utils/hgConfCatalog/hgConfCatalog.py
CATALOG=$CHECKOUT/$RELPATH
PUSH=${HGCONF_NIGHTLY_PUSH:-yes}

fail() { echo "nightlyRegister: $*" >&2; exit 1; }

[[ -d $CHECKOUT/.git ]] || fail "no checkout at $CHECKOUT"
[[ -x $CATALOG ]] || fail "no hgConfCatalog.py at $CATALOG"

cd "$CHECKOUT"

# Rule 3.  A running build owns this tree; come back tomorrow.
if [[ -f $LOCKFILE ]]; then
    lock_pid=$(cat "$LOCKFILE" 2>/dev/null || echo)
    if [[ -n "$lock_pid" ]] && kill -0 "$lock_pid" 2>/dev/null; then
        exit 0
    fi
fi

# Rule 1.  Not master, not our business.  Silent because mid-build is a normal
# state for this tree, not an error worth mailing about every night.
branch=$(git rev-parse --abbrev-ref HEAD)
if [[ $branch != master ]]; then
    exit 0
fi

# Rule 2.  From here on, any exit that has not committed puts the file back the
# way the build expects to find it.
committed=no
cleanup() {
    if [[ $committed == no ]]; then
        git checkout -- "$RELPATH" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Only this one file is ours to touch.  Anything else already modified means
# somebody is working here, and the build is about to complain about it anyway.
others=$(git status --porcelain -- . | grep -v '^?? ' | grep -v " $RELPATH$" || true)
if [[ -n $others ]]; then
    fail "$CHECKOUT has local changes outside the catalog:
$others"
fi

# Start from what master actually says.  Without this, rows written last night
# are still here, the writer counts them as already registered, and the commit
# turns into a pile of yesterdays.
git checkout -- "$RELPATH" 2>/dev/null || true
git fetch --quiet origin master
git merge --quiet --ff-only origin/master \
    || fail "cannot fast-forward $CHECKOUT to origin/master; sort it out by hand"

export KENT_SRC=$CHECKOUT/src

work=$(mktemp -d)
trap 'cleanup; rm -rf "$work"' EXIT

# The two mechanical passes.  Both are deterministic and neither invents a
# judgement, so a cron may run them unattended: --auto-register copies facts off
# the call, and --fix-citations only ever moves a file:line onto the read it
# already names.
"$CATALOG" --auto-register  > "$work/register" 2>&1
"$CATALOG" --fix-citations  > "$work/citations" 2>&1

# What is left after the machine has done all it honestly can.  Non-zero
# whenever a row is waiting to be classified, so the exit code is information.
"$CATALOG" --reconcile > "$work/reconcile" 2>&1 || true

if git diff --quiet -- "$RELPATH"; then
    # Nothing to write down.  Still speak up if reconcile found something the
    # machine cannot fix on its own, since that is the whole point of running.
    if [[ -s $work/reconcile ]]; then
        echo "hg.conf catalog: nothing to register, but --reconcile has notes:"
        cat "$work/reconcile"
    fi
    exit 0
fi

# Name the settings in the subject line so the commit reads like a person wrote
# it, and keep it under a sensible width when there are many.
names=$(git diff -U0 -- "$RELPATH" | sed -n 's/^+ *h("\([^"]*\)".*/\1/p' | paste -sd, - )
count=$(echo "$names" | tr ',' '\n' | grep -c . || true)
if [[ -z $names ]]; then
    subject="hgConfCatalog: refresh the file:line citations, refs #37925"
elif [[ ${#names} -le 60 ]]; then
    subject="hgConfCatalog: register $names, refs #37925"
else
    subject="hgConfCatalog: register $count settings the tree reads, refs #37925"
fi

{
    echo "$subject"
    echo
    echo "Written by nightlyRegister.sh, which records the settings the tree"
    echo "reads that the catalog was missing, and repairs file:line citations"
    echo "whose line numbers have drifted.  Only facts copied off the call site"
    echo "are filled in.  No classification is guessed: a new boolean gets no"
    echo "role=, because calling a release gate a knob would hide it from the"
    echo "sunset report for good, and every row lands in the 'Awaiting review'"
    echo "section until somebody reads the call site."
    echo
    sed 's/^/  /' "$work/register"
    # Only mention citations when some actually moved; "no citation to fix" is
    # not news worth carrying in a commit message forever.
    if [[ -s $work/citations ]] && ! grep -q '^no citation to fix$' "$work/citations"; then
        sed 's/^/  /' "$work/citations"
    fi
} > "$work/msg"

git add -- "$RELPATH"
git commit --quiet --file "$work/msg"
committed=yes

if [[ $PUSH == yes ]]; then
    if ! git push --quiet origin master 2>"$work/pusherr"; then
        # Somebody landed something between the fetch and now.  The work is
        # regenerable, so drop it rather than leave an unpushed commit sitting
        # in the build's tree where the next build would carry it along.
        git reset --hard --quiet origin/master
        committed=yes   # tree is clean again; nothing for the trap to undo
        echo "hg.conf catalog: push rejected, dropped the commit and will redo it tomorrow."
        cat "$work/pusherr"
        exit 0
    fi
fi

echo "hg.conf catalog: $subject"
echo
sed 's/^/  /' "$work/register"
if [[ -s $work/citations ]] && ! grep -q '^no citation to fix$' "$work/citations"; then
    sed 's/^/  /' "$work/citations"
fi
echo
echo "Left for a person to decide:"
sed 's/^/  /' "$work/reconcile"
exit 0

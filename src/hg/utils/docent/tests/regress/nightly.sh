#!/bin/bash
# Nightly Docent regression run against genome-test, for cron.  refs #38252
#
# Mails a report every night whether anything failed or not, on purpose and to match
# the catalogNightly job: no mail means the cron itself has stopped, rather than
# meaning the browser is fine.  Always exits 0, so cron does not send a second mail
# of its own on top of this one.
#
# Runs the tests that are COMMITTED, listed from git rather than from the directory.
# The directory also holds work in progress -- scripts written against a ticket whose
# recipe is not right yet -- and those must not mail a failure every night.  A newly
# committed test is picked up with no edit here.
#
# It runs out of whatever checkout this script itself lives in, so a copy in a second
# checkout needs no argument.  The cron runs it out of a clone of its own,
#
#     /hive/users/braney/docentNightly/kent
#
# the way catalogNightly has one under /hive/users/braney, so that an ordinary day's
# editing in ~/kent cannot change what the cron measures.  Nothing here is built, so
# the clone needs no submodules and no make.
#
# When an xfail PASSES -- the fix reached the server -- that flip is appended to
# flips.log beside the logs, one line per script ever.  It is the only place this job's
# best evidence is kept: the log it came from is deleted after 60 days, and a mail is
# not a record.  See `make proof` in this directory for what the flips are for.
#
# --update brings that clone to origin/master and then re-runs this script from the
# result.  Two reasons for the re-exec rather than a separate driver script beside the
# crontab: the whole job stays in the tree where it can be reviewed and committed, and
# a change to this file takes effect the same night as a change to a test, instead of
# a night later.
#
# --update will only ever touch a checkout that is a pristine mirror of origin/master.
# A working tree with uncommitted edits, or one holding a commit that has not been
# pushed, is left alone and reported, because a `reset --hard` there would throw away
# work.  That is what makes it safe for the flag to exist in a script that also sits in
# a working tree.

set -u
export PATH=/usr/bin:/bin:/usr/local/bin:$PATH

HERE=$(cd "$(dirname "$0")" && pwd)
PW=/hive/groups/browser/uiTest/pw
PW_ENV="env PLAYWRIGHT_BROWSERS_PATH=$PW/browsers NODE_PATH=$PW/node_modules"
TO=${DOCENT_NIGHTLY_TO:-braney@ucsc.edu}
LOGDIR=${DOCENT_NIGHTLY_LOGS:-/hive/users/braney/docentNightly/logs}
FLIPS=${DOCENT_NIGHTLY_FLIPS:-/hive/users/braney/docentNightly/flips.log}
STAMP=$(date +%Y-%m-%d_%H%M)
# Only a label for the flips.log line.  Every script here says `target: genome-test`
# itself; this is not read from them, so override it if that ever stops being true.
TARGET=${DOCENT_NIGHTLY_TARGET:-genome-test}

mkdir -p "$LOGDIR"
OUT="$LOGDIR/$STAMP.txt"
VERDICTS="$LOGDIR/.$STAMP.verdicts"
trap 'rm -f "$VERDICTS"' EXIT

# A job that could not start has to arrive looking like the others, or a night when
# nothing ran reads as a quiet night.  Same subject shape, same log file, exit 0.
bail() {
{
    echo "Docent regression run, $(date)"
    echo "checkout: $HERE"
    echo
    echo "$@"
    echo
    echo "Nothing was tested.  This is a problem with this job, not with the browser."
} > "$OUT" 2>&1
mail -s "docent regression: BROKEN (0 ok) $(date +%F)" "$TO" < "$OUT"
exit 0
}

if [ "${1:-}" = --update ] && [ -z "${DOCENT_NIGHTLY_UPDATED:-}" ]; then
    shift
    dirty=$(git -C "$HERE" status --porcelain --untracked-files=no 2>&1)
    if [ -n "$dirty" ]; then
        bail "--update will not reset $HERE: it has uncommitted changes.

$dirty"
    fi
    git -C "$HERE" fetch -q origin master 2>&1 || \
        bail "--update could not fetch origin master into $HERE."
    ahead=$(git -C "$HERE" rev-list --oneline FETCH_HEAD..HEAD 2>&1)
    if [ -n "$ahead" ]; then
        bail "--update will not reset $HERE: it holds commits that are not on origin/master.

$ahead"
    fi
    git -C "$HERE" reset -q --hard FETCH_HEAD 2>&1 || \
        bail "--update could not move $HERE to FETCH_HEAD."
    # Re-open this file by name, which is now the copy that just arrived.
    export DOCENT_NIGHTLY_UPDATED=1
    exec "$0" "$@"
fi

# The committed scripts, as bare test names.  `make test T=` takes a list.
TESTS=$(cd "$HERE" && git ls-files '*.docent.yaml' 2>/dev/null \
        | sed 's#.*/##; s#\.docent\.yaml$##' | tr '\n' ' ')

{
  echo "Docent regression run, $(date)"
  echo "checkout: $HERE"
  echo "branch:   $(git -C "$HERE" rev-parse --abbrev-ref HEAD 2>/dev/null)"
  echo "commit:   $(git -C "$HERE" rev-parse --short HEAD 2>/dev/null)"
  if [ -n "${DOCENT_NIGHTLY_UPDATED:-}" ]; then
    echo "updated:  this checkout was reset to origin/master before the run"
  fi
  echo

  if [ -z "${TESTS// /}" ]; then
    echo "NO COMMITTED TESTS FOUND -- git ls-files returned nothing in $HERE."
    echo "That is a problem with this job, not with the browser."
    subject_state="BROKEN"
  else
    echo "tests: $TESTS"
    echo
    echo "--- preflight (the sessions and hubs these tests depend on) ---"
    # Reported separately from the tests on purpose.  A session that has been deleted
    # or a hub that has moved is not a browser regression, and the two must not arrive
    # as the same red.
    #
    # preflight is given the committed list rather than left to scan the directory, so a
    # dead fixture belonging to a work-in-progress script is not reported as a problem
    # with a run that never included it.
    if (cd "$HERE" && $PW_ENV node ../preflight.js . $TESTS 2>&1); then
      pf=ok
    else
      pf=MISSING
    fi
    echo
    echo "--- tests ---"
    # Written to a file of its own and then echoed, rather than straight into the
    # block's redirect, because the flip scan below has to read the verdicts back and
    # $OUT is the file this block is being written to.
    if (cd "$HERE" && make test T="$TESTS" > "$VERDICTS" 2>&1); then
      tests=pass
    else
      tests=FAIL
    fi
    cat "$VERDICTS"

    if [ "$tests" = FAIL ] && [ "$pf" = MISSING ]; then
      subject_state="FAIL (fixtures missing too)"
    elif [ "$tests" = FAIL ]; then
      subject_state="FAIL"
    elif [ "$pf" = MISSING ]; then
      subject_state="pass, but a fixture is missing"
    else
      subject_state="pass"
    fi
  fi

  # An xfail that PASSED is the one piece of evidence this job produces that cannot be
  # got any other way: the same server, the same fixtures, the same script, one real
  # build apart.  That is what separates a test that has been watched to fail for its
  # own reason from a test that only asserts the answer -- see `make proof`.  Until now
  # it arrived as a red mail and was then thrown away, so record it before the log ages
  # out, and say what to do with it.
  #
  # The record lives OUTSIDE the checkout on purpose.  --update does `reset --hard`, so
  # anything written into the tree here is gone the next night.
  #
  # One line per script, ever.  Without the dedupe this would append every night from
  # the flip until the promotion commit lands, which is exactly the stretch when nobody
  # is looking.
  flipped=$(awk '/^=== /{n=$2} /supposed to fail, and it passed/{print n}' "$VERDICTS" 2>/dev/null)
  if [ -n "$flipped" ]; then
    echo
    echo "--- flips (an xfail passed, so the fix reached this server) ---"
    for t in $flipped; do
      if [ -f "$FLIPS" ] && awk -v t="$t" '$2 == t { found = 1 } END { exit !found }' "$FLIPS"; then
        echo "  $t -- already recorded in $FLIPS"
      else
        printf '%s %-22s passed on %s  commit=%s  log=%s\n' \
          "$(date +%F)" "$t" "$TARGET" \
          "$(git -C "$HERE" rev-parse --short HEAD 2>/dev/null)" "$STAMP.txt" >> "$FLIPS"
        echo "  $t -- recorded in $FLIPS"
      fi
    done
    echo
    echo "  To close one out: drop the .xfail from the script's name, and add a"
    echo "  server-flip line to its proof: key naming the commit that shipped the fix."
    echo "  Then \`make proof\` counts it.  Until that commit is pushed this stays red."
  fi

  echo
  echo "full log kept at $OUT"
} > "$OUT" 2>&1

# subject_state is set inside the block above, which runs in this shell, so it survives.
n=$(grep -c '^  ok$' "$OUT" 2>/dev/null || echo 0)
mail -s "docent regression: ${subject_state:-unknown} ($n ok) $(date +%F)" "$TO" < "$OUT"

# Keep two months of logs and no more.
find "$LOGDIR" -name '*.txt' -mtime +60 -delete 2>/dev/null

exit 0

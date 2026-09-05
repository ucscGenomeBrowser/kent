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
# It runs out of whatever checkout this script itself lives in, so a second copy in a
# second checkout needs no argument.  While the tests live only on branch
# docentTests37892 that means Brian's worktree.  Once the branch is on master this
# should move to a checkout of its own, the way catalogNightly has one under
# /hive/users/braney, so that an ordinary day's editing cannot change what the cron
# measures.

set -u
export PATH=/usr/bin:/bin:/usr/local/bin:$PATH

HERE=$(cd "$(dirname "$0")" && pwd)
PW=/hive/groups/browser/uiTest/pw
PW_ENV="env PLAYWRIGHT_BROWSERS_PATH=$PW/browsers NODE_PATH=$PW/node_modules"
TO=${DOCENT_NIGHTLY_TO:-braney@ucsc.edu}
LOGDIR=${DOCENT_NIGHTLY_LOGS:-/hive/users/braney/docentNightly/logs}
STAMP=$(date +%Y-%m-%d_%H%M)

mkdir -p "$LOGDIR"
OUT="$LOGDIR/$STAMP.txt"

# The committed scripts, as bare test names.  `make test T=` takes a list.
TESTS=$(cd "$HERE" && git ls-files '*.docent.yaml' 2>/dev/null \
        | sed 's#.*/##; s#\.docent\.yaml$##' | tr '\n' ' ')

{
  echo "Docent regression run, $(date)"
  echo "checkout: $HERE"
  echo "branch:   $(git -C "$HERE" rev-parse --abbrev-ref HEAD 2>/dev/null)"
  echo "commit:   $(git -C "$HERE" rev-parse --short HEAD 2>/dev/null)"
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
    if (cd "$HERE" && make test T="$TESTS" 2>&1); then
      tests=pass
    else
      tests=FAIL
    fi

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

  echo
  echo "full log kept at $OUT"
} > "$OUT" 2>&1

# subject_state is set inside the block above, which runs in this shell, so it survives.
n=$(grep -c '^  ok$' "$OUT" 2>/dev/null || echo 0)
mail -s "docent regression: ${subject_state:-unknown} ($n ok) $(date +%F)" "$TO" < "$OUT"

# Keep two months of logs and no more.
find "$LOGDIR" -name '*.txt' -mtime +60 -delete 2>/dev/null

exit 0

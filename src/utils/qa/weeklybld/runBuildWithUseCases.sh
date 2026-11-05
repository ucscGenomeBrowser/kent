#!/bin/bash
# Nightly build and "make test", run from the build user's crontab.
# On a failure this script mails the buildmeister and QA itself, and prints
# nothing, so cron does not also mail the crontab's MAILTO list.

qaEmail=browser-qa@soe.ucsc.edu
buildmeister=${BUILDMEISTEREMAIL:-$(tcsh -c 'echo $BUILDMEISTEREMAIL' 2>/dev/null | tail -1)}
buildmeister=${buildmeister:-braney@ucsc.edu}

kentSrc=$HOME/kent/src
buildLog=$HOME/kent/build.log
testLog=$kentSrc/makeTest.log
useLog=$kentSrc/buildWithUse.log
# HEAD of the last night that built and tested cleanly, for the commit list
lastClean=$HOME/kent/lastCleanNightly

(cd $kentSrc && make clean > /dev/null 2>&1)
$HOME/bin/scripts/buildWithUseCases $kentSrc alpha > $buildLog 2>&1
ret=$?

head=$(git -C $kentSrc rev-parse --short=11 HEAD 2>/dev/null)
if [ $ret -eq 0 ]; then
    echo $head > $lastClean
    exit 0
fi

failedTests() {
# Print "directory: target" for each failing make target in the test log,
# leaving out the parent makes that only failed because a child did.
awk '
/^make\[[0-9]+\]: Entering directory/ {
    level = $1; gsub(/[^0-9]/, "", level)
    dir = $0; sub(/^[^'\'']*'\''/, "", dir); sub(/'\''$/, "", dir)
    dirs[level] = dir
}
/^make\[[0-9]+\]: \*\*\* \[.*\] Error [0-9]+$/ {
    level = $1; gsub(/[^0-9]/, "", level)
    dir = dirs[level]
    target = $0; sub(/^.*\[[^:]*:[0-9]+: /, "", target); sub(/\] Error.*$/, "", target)
    for (i = 1; i <= n; i++)
        if (index(seen[i], dir "/") == 1)
            next
    seen[++n] = dir
    sub("^" src "/", "", dir)
    printf "%s: %s\t%d\n", dir, target, NR
}' src=$kentSrc $testLog
}

body=$(mktemp /data/tmp/nightlyBuild.XXXXXX)
{
echo "The nightly build on $(hostname -s) failed."
echo "Tree: $kentSrc at $head"
echo "Date: $(date)"
echo
if grep -q "Error during make test." $buildLog; then
    failures=$(failedTests)
    count=$(echo "$failures" | grep -c .)
    subject="Nightly build: $count make test failure(s) on $(hostname -s) at $head"
    echo "The code built, and $count test target(s) failed in make test:"
    echo
    echo "$failures" | cut -f1 | sed 's/^/    /'
    echo
    echo "$failures" | while IFS=$'\t' read name line; do
        echo "==== $name, makeTest.log line $line ===="
        start=$((line > 25 ? line - 25 : 1))
        sed -n "${start},${line}p" $testLog
        echo
    done
    echo "Full log: $testLog"
else
    subject="Nightly build: build FAILED on $(hostname -s) at $head"
    echo "The build failed in these USE cases:"
    echo
    grep -F '*** ERROR stopped' $useLog | sed 's/^/    /'
    echo
    echo "First errors in the build log:"
    echo
    grep -E 'error:|\*\*\* \[' $buildLog | head -20 | sed 's/^/    /'
    echo
    echo "Full logs: $buildLog and $useLog"
fi
if [ -s $lastClean ]; then
    echo
    echo "Commits since the last clean night ($(cat $lastClean)):"
    echo
    git -C $kentSrc log --no-merges --format='    %h %an: %s' $(cat $lastClean)..HEAD | head -60
fi
} > $body 2>&1

if ! mail -s "$subject" $buildmeister $qaEmail < $body; then
    # mail failed, so let cron send it instead
    echo "$subject"
    cat $body
fi
rm -f $body

#!/bin/sh
# configuration setup and cron wrapper for the doUniprot script

cd /hive/data/outside/otto/uniprot || exit 1
umask 002

#echo WARNING: NOT DOWNLOADING
#./doUniprot run --skipDownload

runLog=runLog.txt

logRun() {
    echo "`date '+%Y-%m-%d %H:%M:%S'` $*" >> $runLog
}

# activate the python environment that has the lxml XML parser. Rebuild it with
# ./makeVenv.sh if this fails.
if [ ! -f venv/bin/activate ] ; then
    logRun "PREFLIGHT-FAIL no venv"
    echo "UniProt update did not start: venv/bin/activate is missing."
    echo "Rebuild it with: cd /hive/data/outside/otto/uniprot && ./makeVenv.sh"
    exit 1
fi
. venv/bin/activate

# Do not spend 35 minutes downloading UniProt only to find out that the parser cannot
# start. Run it with --help, which imports lxml and then exits, and stop here if that
# fails. Invoked exactly the way doUniprot invokes it, so this tests the same python.
if ! ./uniprotToTab --help > /dev/null 2>&1; then
    logRun "PREFLIGHT-FAIL uniprotToTab cannot start"
    echo "UniProt update did not start: ./uniprotToTab cannot be run."
    echo
    echo "The lxml python module does not import. Rebuild the environment with:"
    echo "    cd /hive/data/outside/otto/uniprot && ./makeVenv.sh"
    echo
    ./uniprotToTab --help 2>&1 | tail -20
    exit 1
fi

# A killed run would otherwise leave a START with no matching line, which reads the same
# as a run that is still going. Say it was interrupted, and drop the lock file, which
# doUniprot's own atexit handler does not get to run on a signal.
trap 'logRun "INTERRUPTED killed by a signal"; rm -f /hive/data/outside/uniProt/current/doUniprot.lock; exit 130' INT TERM HUP

logRun "START"
./doUniprot run > lastRun.log 2>&1
exitCode=$?
trap - INT TERM HUP
logRun "END exit=$exitCode"

if grep -q "Is a doUniprot process already running" lastRun.log ; then
    # A run from last month, or a hand-started one, is still going, or crashed and left
    # its lock file behind. Say so in one line instead of the failure report below: this
    # is not a broken pipeline, but a stale lock does need someone to look at it.
    logRun "LOCKED another doUniprot run holds the lock file"
    echo "UniProt update skipped: another doUniprot run holds the lock file."
    echo "If nothing is running, remove /hive/data/outside/uniProt/current/doUniprot.lock"
    exit 0
fi

if [ $exitCode -ne 0 ] ; then
    # lastRun.log is overwritten by the next run, so keep a copy. Without one, a
    # failure that nobody reads leaves no trace on disk at all.
    cp -f lastRun.log lastFail.log
    logRun "FAIL exit=$exitCode log=lastFail.log"
    echo "Big UniProt update FAILED, exit code $exitCode"
    echo
    echo "Full log: /hive/data/outside/otto/uniprot/lastFail.log"
    echo "Restart manually with:"
    echo "    cd /hive/data/outside/otto/uniprot && ./doUniprot run"
    echo "usually with the -p option to skip download and parsing of the gigantic XML."
    echo
    echo "Last 25 lines of the log:"
    tail -25 lastFail.log
    exit $exitCode
fi

if grep -q "are not newer than file in" lastRun.log ; then
    # UniProt had no new release this month. This is the normal case for most months,
    # so stay silent: otto crons only mail when something changed or something broke.
    logRun "NOCHANGE no new UniProt release on the server"
    exit 0
fi

logRun "OK updated to `cat tab/version.txt`"
echo "Big UniProt update OK"
echo "Now serving: `cat tab/version.txt`"

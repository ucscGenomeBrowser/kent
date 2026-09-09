#!/bin/sh
# configuration setup and cron wrapper for the doUniprot script

cd /hive/data/outside/otto/uniprot || exit 1
umask 002

#echo WARNING: NOT DOWNLOADING
#./doUniprot run --skipDownload

# There is deliberately no virtualenv here anymore. uniprotToTab needs the lxml XML
# parser, which on hgwdev comes from the system package python3-lxml and is upgraded
# together with /usr/bin/python3. A virtualenv used to sit in venv/ instead, but its
# python was only a symlink to /usr/bin/python3: when the system python moved from 3.6
# to 3.9 the compiled lxml in the venv stopped loading, every monthly run died at the
# parse step, and the tracks stayed on release 2024_06 for 19 months (redmine #38300).

runLog=runLog.txt

logRun() {
    echo "`date '+%Y-%m-%d %H:%M:%S'` $*" >> $runLog
}

# Do not spend 35 minutes downloading UniProt only to find out that the parser cannot
# start. Run it with --help, which imports lxml and then exits, and stop here if that
# fails. Invoked exactly the way doUniprot invokes it, so this tests the same python.
if ! ./uniprotToTab --help > /dev/null 2>&1; then
    logRun "PREFLIGHT-FAIL uniprotToTab cannot start"
    echo "UniProt update did not start: ./uniprotToTab cannot be run."
    echo
    echo "Almost certainly the lxml python module is missing. Check with:"
    echo "    python3 -c 'import lxml.etree'"
    echo "and see /hive/data/outside/otto/uniprot/README.txt for how to repair it."
    echo
    ./uniprotToTab --help 2>&1 | tail -20
    exit 1
fi

logRun "START"
./doUniprot run > lastRun.log 2>&1
exitCode=$?
logRun "END exit=$exitCode"

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

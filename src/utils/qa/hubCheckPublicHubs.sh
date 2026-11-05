#!/usr/bin/env bash

# Program Header
# Name:   Gerardo Perez
# Description: A program that runs hubCheck for all the public hubs on the RR and outputs it into a file
#
# hubCheckPublicHubs.sh
#

# Runtime limits. A hung HTTPS fetch inside hubCheck can block forever with no timeout of its
# own (refs #38366), so cap each hubCheck call and cap the run as a whole. Without these a
# single unresponsive hub wedges the monthly check indefinitely and no hub after it is seen.
hubTimeout=${HUB_TIMEOUT:-5m}
runTimeoutSecs=${RUN_TIMEOUT_SECS:-21600}   # 6 hours for the whole loop
archiveRoot=${ARCHIVE_ROOT:-/hive/users/qateam/hubCheckCronArchive}
hubCheck=${HUBCHECK:-/cluster/bin/x86_64/hubCheck}

archiveDir=$archiveRoot/`date +'%Y-%m'`
mkdir -p $archiveDir
outputFile=$archiveDir/hubCheck_output

# Only let one run exist at a time. A wedged run is still appending to this month's output file
# when the next cron fires, and two runs interleaving their output corrupts the record for
# hubCheckDraftEmails.py, which reads the file positionally.
exec 9>$archiveRoot/.hubCheckPublicHubs.lock
if ! flock -n 9
then
    echo "hubCheckPublicHubs.sh: another run is already in progress, exiting." 1>&2
    exit 1
fi

echo '#############################################' >> $outputFile

checked=0
timedOut=0
skipped=0

# Note: no "tail -n +2" here. hgsql -N emits no header row, so that was silently dropping the
# first public hub from every run.
hubUrls=$(/cluster/bin/x86_64/hgsql -h genome-centdb -Ne "select hubUrl from hubPublic" hgcentral)
for output in $hubUrls
do
    # The line directly after a ##### delimiter has to stay the hub URL. hubCheckDraftEmails.py
    # maps an error back to its hub with "grep -A 1 '####' | tail -1", so nothing may be
    # inserted between the delimiter and the URL.
    echo $output >> $outputFile

    if [ $SECONDS -ge $runTimeoutSecs ]
    then
        echo "hubCheck skipped: overall run limit of ${runTimeoutSecs}s reached" >> $outputFile
        skipped=$((skipped + 1))
    else
        timeout -k 30s $hubTimeout $hubCheck "$output" 2> /dev/null >> $outputFile
        rc=$?
        checked=$((checked + 1))
        # 124 is timeout's own TERM; 137 is the SIGKILL that -k sends if TERM was ignored.
        if [ $rc -eq 124 ] || [ $rc -eq 137 ]
        then
            echo "hubCheck timed out after $hubTimeout and was killed (exit $rc)" >> $outputFile
            timedOut=$((timedOut + 1))
        fi
    fi
    echo '#############################################' >> $outputFile
done

# Summary goes to stderr so it lands in the cron mail rather than in the parsed output file.
echo "hubCheckPublicHubs.sh: $checked checked, $timedOut timed out, $skipped skipped" 1>&2

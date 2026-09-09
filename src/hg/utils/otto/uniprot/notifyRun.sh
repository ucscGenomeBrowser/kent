#!/bin/sh
# Mail a note every time a hand-started doUniprot run reaches a new stage, and once more
# when it ends. For a run that takes days, this is the difference between knowing where it
# is and having to go look.
#
#   cd /hive/data/outside/otto/uniprot
#   setsid nohup ./notifyRun.sh you@ucsc.edu > notifyRun.log 2>&1 < /dev/null &
#
# The cron run does not need this: doUpdate.sh already mails the outcome through the
# MAILTO line in otto's crontab. This is for watching a long catch-up run in between.

to=${1:-$USER@soe.ucsc.edu}
poll=${2:-300}          # seconds between checks
heartbeat=${3:-43200}   # seconds between "still going" notes when the stage does not change

dir=/hive/data/outside/otto/uniprot
log=$dir/lastRun.log
lock=/hive/data/outside/uniProt/current/doUniprot.lock

send() {
    subject=$1
    { echo "run directory: $dir"
      echo "started:       $started"
      echo "now:           `date '+%Y-%m-%d %H:%M:%S'`"
      echo
      echo "last runLog.txt lines:"
      tail -5 $dir/runLog.txt 2>/dev/null
      echo
      echo "last log lines:"
      grep -a " - " $log 2>/dev/null | tail -12
    } | mail -s "uniprot otto: $subject" "$to"
}

# Work out which stage the log has reached. Later matches win, so the order is the order
# the pipeline goes through.
stageOf() {
    s="starting up"
    grep -aq "Downloading NCBI gene2refseq"  $log 2>/dev/null && s="downloading the NCBI gene2refseq file"
    grep -aq "lftp ftp"                      $log 2>/dev/null && s="mirroring UniProt from expasy"
    grep -aq "Moving files from"             $log 2>/dev/null && s="download finished, moving files into place"
    grep -aq "not newer than file in"        $log 2>/dev/null && s="no new UniProt release, nothing to do"
    grep -aq "Converting uniprot XML"        $log 2>/dev/null && s="parsing the SwissProt XML"
    grep -aq -- "--trembl"                   $log 2>/dev/null && s="parsing the TrEMBL XML, this is the multi-day part"
    grep -aq "checking/creating pslMap"      $log 2>/dev/null && s="parse done, building the protein-to-genome mappings on the cluster"
    grep -aq "Wrote release string"          $log 2>/dev/null && s="writing version files and flipping the bigBeds"
    grep -aq "Archive: Copied"               $log 2>/dev/null && s="copying to the hgdownload archive"
    grep -aq "Uniprot pipeline completed"    $log 2>/dev/null && s="finished"
    echo "$s"
}

running() {
    pgrep -f "doUniprot run" > /dev/null 2>&1
}

if ! running ; then
    echo "No doUniprot run in progress, nothing to watch."
    exit 1
fi

started=`date '+%Y-%m-%d %H:%M:%S'`
stage=`stageOf`
lastMail=`date +%s`
send "run being watched, now $stage"

while running ; do
    sleep $poll
    now=`stageOf`
    if [ "$now" != "$stage" ] ; then
        stage=$now
        lastMail=`date +%s`
        send "$stage"
    elif [ $((`date +%s` - lastMail)) -ge $heartbeat ] ; then
        lastMail=`date +%s`
        send "still $stage"
    fi
done

# The process is gone. runLog.txt says how it went, since doUpdate.sh writes the outcome
# there whether it succeeded, failed or was interrupted.
sleep 10
outcome=`grep -aE " (OK|FAIL|NOCHANGE|INTERRUPTED|LOCKED) " $dir/runLog.txt 2>/dev/null | tail -1`
case "$outcome" in
    *" OK "*)          send "FINISHED, update succeeded" ;;
    *" NOCHANGE "*)    send "finished, there was no new release to load" ;;
    *" INTERRUPTED "*) send "STOPPED, the run was killed" ;;
    *" FAIL "*)        send "FAILED, see lastFail.log" ;;
    *)                 send "run is gone and runLog.txt does not say why, please look" ;;
esac

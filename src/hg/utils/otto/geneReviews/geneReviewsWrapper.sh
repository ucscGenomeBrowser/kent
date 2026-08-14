#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="otto-group@ucsc.edu"
WORKDIR="/hive/data/outside/otto/geneReviews"
LOG="$WORKDIR/lastRun.log"

umask 002
cd $WORKDIR

# Run to a log first, so the exit status can pick the subject.  A failed run used
# to arrive with the same "GENEREVIEW Build" subject as a good one, and opened
# with the same routine build output, so six weeks of failures read like six
# weeks of normal updates.  See #38098.
./checkGeneReviews.sh $WORKDIR > $LOG 2>&1 && rc=0 || rc=$?

if [ $rc -eq 0 ]
then
    # A week where NCBI posts nothing produces no output at all.  mail -E drops
    # the empty message, which is how the job stays quiet with nothing to say.
    mail -E -s "GENEREVIEW Build" $EMAIL < $LOG
else
    # Send on failure even if the run somehow produced no output.
    echo "checkGeneReviews.sh exited $rc" >> $LOG
    mail -s "GENEREVIEW Build FAILED" $EMAIL < $LOG
fi

exit $rc

#!/bin/csh -ef
# Load a list of sample ids from a file through signal file post-processing
#
# Input file:  load.<tag>.ids
# Creates directory <tag> for files, and file load.<tag>.log

set file = $1
if ($file == "") then
    echo "usage: loadMany.csh load.<tag>.ids"
    exit 1
endif
if (! -r $file) then
    echo "can't read file: $file"
    exit 1
endif

set run = $file:r:e
set logfile = load.$run.log

if (-f $logfile) then
    echo "ERROR: log file $logfile exists"
    exit 1
endif

# TODO: move these to a shared (sourced) file
set indir = /hive/data/outside/gtex/signal/in/$run
set bindir = /hive/data/outside/gtex/signal/bin

set count = `cat $file | wc -l`
echo "Started load: $run of $count signals at `date`, output to $indir" | tee -a $logfile

set ids = `awk '{printf("%s.%s\n", $1, $2)}' $file`
foreach id ($ids)
    csh $bindir/loadOne.csh $id $run >> $logfile
    sleep 20
end
grep ERROR $logfile
set errs = `grep -c ERROR $logfile`
echo "Finished load: $run of $count signals at `date` with $errs errors" | tee -a $logfile
set runstat = "ERROR"
if ($errs == 0) then
    set runstat = "OK"
endif

mail -s "GTEx signal pipeline: LOAD $run status: $runstat" kate pnejad@ucsc.edu < $logfile

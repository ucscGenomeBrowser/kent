#!/bin/csh -ef
# Run a list of sample ids from a file through signal file post-processing
#
# Input file:  run.<tag>.ids
# Creates directory <tag> for output files, and file run.<tag>.log

set file = $1
if ($file == "") then
    echo "usage: runMany.csh run.<tag>.ids"
    exit 1
endif
if (! -r $file) then
    echo "can't read file: $file"
    exit 1
endif

set run = $file:r:e
set logfile = run.$run.log

if (-f $logfile) then
    echo "ERROR: log file $logfile exists"
    exit 1
endif

# TODO: move these to a shared (sourced) file
set outdir = /hive/data/outside/gtex/signal/out/$run
set bindir = /hive/data/outside/gtex/signal/bin

set count = `cat $file | wc -l`
echo "Started run: $run of $count signals at `date`, output to $outdir" | tee -a $logfile

set ids = `awk '{printf("%s.%s\n", $1, $2)}' $file`
foreach id ($ids)
    csh $bindir/runOne.csh $id $run >> $logfile
end
grep ERROR $logfile
set errs = `grep -c ERROR $logfile`
echo "Finished run: $run of $count signals at `date` with $errs errors" | tee -a $logfile
set runstat = "ERROR"
if ($errs == 0) then
    set runstat = "OK"
endif

mail -s "GTEx signal pipeline: RUN $run status: $runstat" kate pnejad@ucsc.edu < $logfile

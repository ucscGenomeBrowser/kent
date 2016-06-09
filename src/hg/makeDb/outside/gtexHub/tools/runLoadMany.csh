#!/bin/csh -ef
# Run previously downloaded gzipped tarballs in a directory  through signal file post-processing
#
# Input directory:  in/<run>
# Input data files are SRR*.tar.gz
# Creates directory out/<run> for output files, and file runload.<run>.log

set run = $1
if ($run == "") then
    echo "usage: runLoadMany.csh <run>"
    exit 1
endif

set logfile = runload.$run.log

if (-f $logfile) then
    echo "ERROR: log file $logfile exists"
    exit 1
endif

# TODO: move these to a shared (sourced) file
set bindir = /hive/data/outside/gtex/signal/bin
set indir = /hive/data/outside/gtex/signal/in/$run
set outdir = /hive/data/outside/gtex/signal/out/$run

if (! -d $indir) then
    echo "can't read directory: $indir"
    exit 1
endif
set count = `ls $indir/SRR*.tar.gz | wc -l`
echo "Started runload: $run of $count signals at `date`, output to $outdir" | tee -a $logfile

set files = `ls $indir/SRR*.tar.gz`
foreach file ($files)
    set id = $file:t:r:r
    csh $bindir/runLoadOne.csh $id $run >> $logfile
end
grep ERROR $logfile
set errs = `grep -c ERROR $logfile`
echo "Finished runload: $run of $count signals at `date` with $errs errors" | tee -a $logfile
set runstat = "ERROR"
if ($errs == 0) then
    set runstat = "OK"
endif

mail -s "GTEx signal pipeline: RUNLOAD $run status: $runstat" kate pnejad@ucsc.edu < $logfile

#!/bin/csh -ef
# Link signal files in a directory to hub
#

set run = $1
if ($run == "") then
    echo "usage: linkMany.csh runId"
    exit 1
endif

set bindir = /hive/data/outside/gtex/signal/bin
set outdir = /hive/data/outside/GTEx/signal/out/$run/hg38
set files = `ls $outdir`
foreach file ($files)
    set id = $file:r:r
    csh $bindir/linkOne.csh $id $run
end


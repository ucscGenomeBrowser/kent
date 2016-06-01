#!/bin/csh -ef
# Link a GTEx signal file into hub dir

set id = $1
set run = $2
if ($id == "" || $run == "") then
    echo "usage: linkOne.csh sampleId runId"
    exit 1
endif

foreach db ("hg38" "hg19")
    set outdir = /hive/data/outside/GTEx/signal/out/$run/$db
    set hubdir = /hive/data/outside/GTEx/signal/hub/$db/$run
    mkdir -p $hubdir
    set link = $hubdir/gtexRnaSignal$id.$db.bigWig
    if (! -r $link) then
        ln -s $outdir/$id.$db.bigWig $link
    endif
end


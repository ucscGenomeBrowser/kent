#!/bin/csh -ef
# Download a GTEx signal file from the cloud using s3 command line
# Files from John Vivian, CGL group (Benedict Paten lead)
# NOTE: user must have Amazon credentials configured into s3cmd.  See AWS SQS instructions.

# for kolossus (or hgwdev)
set id = $1:r
set key = $1:e
set run = $2

if ($id == "" || $run == "") then
    echo "usage: s3loadOne.csh sampleId runId"
    exit 1
endif
set indir = /hive/data/outside/gtex/signal/in/$run
mkdir -p $indir

# signed URLs from Hannes, for the full set 7999.  Keys good for one week
set cloudfile  = "s3://cgl-rnaseq-recompute/gtex/wiggle_files/$id.wiggle.tar.gz"

set infile = $indir/$id.tar.gz

echo -n "$id\t"

# get tarball with wiggles
s3cmd get --quiet --requester-pays $cloudfile $infile

if ($status != 0) then
    echo ERROR
    exit 1
endif
set size = `ls -s $infile | awk '{print $1}'`
echo -n "$size\t$infile:t\t"
echo

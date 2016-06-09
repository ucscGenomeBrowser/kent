#!/bin/csh -ef
# Download a GTEx signal file from the cloud
# From John Vivian, CGL group (Benedict Paten lead)

# for kolossus (or hgwdev)
set id = $1:r
set key = $1:e
set run = $2

if ($id == "" || $run == "") then
    echo "usage: loadOne.csh sampleId runId"
    exit 1
endif
set indir = /hive/data/outside/gtex/signal/in/$run
mkdir -p $indir

# signed URLs from Hannes, for the full set 7999.  Keys good for one week
set url = "https://cgl-rnaseq-recompute.s3-us-west-2.amazonaws.com/gtex/wiggle_files/$id.wiggle.tar.gz?Signature=$key&Expires=1456537090&AWSAccessKeyId=AKIAIG6JFPTZZYB3FBWA&x-amz-request-payer=requester"

set infile = $indir/$id.tar.gz

echo -n "$id\t"

# get bedgraph file (unique+multi signal) from package in amazon cloud
curl -sL "$url" > $infile

if ($status != 0) then
    echo ERROR
    exit 1
endif
set size = `ls -s $infile | awk '{print $1}'`
echo -n "$size\t$infile:t\t"
echo

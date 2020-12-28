#!/bin/bash

set -beEu -o pipefail

# stdin: series of BioSample GI# IDs (numeric IDs, *not* accessions)
# stdout: full text record for each BioSample

url="https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi"
db="biosample"
retmode="text"
tool="bioSampleIdToText"
email="$USER%40soe.ucsc.edu"
baseParams="db=$db&retmode=$retmode&tool=$tool&email=$email"
# Add &id=... for each id in input, request in batches...

batchSize=100

TMPDIR=/dev/shm
paramFile=`mktemp`

initBatch() {
    count=0
    echo -n $baseParams > $paramFile
}

sendBatch() {
    curl -s -S -X POST -d @$paramFile "$url"
    # Give NCBI a rest
    sleep 1
}

initBatch

while read id; do
    echo -n "&id=$id" >> $paramFile
    count=$(expr $count + 1)
    if [ $count == $batchSize ]; then
        sendBatch
        initBatch
    fi
done
if [ $count != 0 ]; then
    sendBatch
fi
rm $paramFile

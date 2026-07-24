#!/bin/bash
set -beEu -o pipefail
D=/hive/data/genomes/hg38/bed/hprc2X
for i in 1 2 3 4 5; do
  bash $D/buildSample.sh $D/samp$i 88 "samp$i" >> $D/samples.build.log 2>&1
  bash $D/dbsnpStats.sh $D/samp$i.canon.tsv 88 "rel2samp$i"
done
echo "ALLDONE"

#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="otto-group@ucsc.edu"
WORKDIR="/hive/data/outside/otto/geneReviews"

cd $WORKDIR
./checkGeneReviews.sh $WORKDIR 2>&1 | mail -E -s "GENEREVIEW Build" $EMAIL
